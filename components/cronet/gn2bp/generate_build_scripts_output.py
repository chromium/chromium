# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Generates the rust build script output into a well-structured JSON format.
"""
import argparse
from typing import Set, List, Dict
import glob
import json
import tempfile
import re
import subprocess
import os
import sys

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))
sys.path.insert(0, REPOSITORY_ROOT)

# The current value of 500 is a heuristic that seems to work. If command
# line length limitation is exceeded, reduce this number.
_MAX_TARGETS_PER_NINJA_EXECUTION = 500

import components.cronet.tools.utils as cronet_utils  # pylint: disable=wrong-import-position
import components.cronet.gn2bp.gen_android_bp as cronet_gn2bp  # pylint: disable=wrong-import-position

# TODO: Move ARCHS to cronet_utils.
_ARCHS = ["x86", "x64", "arm", "arm64", "riscv64"]
# TODO: Move _OUT_DIR to cronet_utils.
_OUT_DIR = os.path.join(REPOSITORY_ROOT, "out")


def _find_all_cargo_flags_files(out_dir: str) -> Set[str]:
    return set(glob.glob(f"{out_dir}/gen/**/cargo_flags.rs", recursive=True))


def _find_all_host_cargo_flags_files(out_dir: str) -> Set[str]:
    return set(
        glob.glob(f"{out_dir}/clang_*/gen/**/cargo_flags.rs", recursive=True))


def _get_args_for_arch(arch: str) -> List[str]:
    default_args = cronet_utils.get_android_gn_args(True, arch)
    return ' '.join(
        cronet_utils.filter_gn_args(default_args,
                                    ["use_remoteexec", "enable_rust"]))


def _build_rust_build_script_actions(out_path: str):
    """Builds build script actions, first GN is used to query
    all actions that are available to build, then the actions are
    filtered to only actions that has "build_script_output" in its
    name which indicates that it builds a build_script.

    The build script actions are split into chunks of _MAX_TARGETS_PER_NINJA_EXECUTION
    where each ninja execution will build _MAX_TARGETS_PER_NINJA_EXECUTION actions.
    This is to avoid hitting the command-line maximum length limitation.

    Args:
      out_path: the GN output directory.

    Raises:
      Exception: If ninja execution has failed or querying GN failed.
    """
    all_actions_process = subprocess.run(
        [cronet_utils.GN_PATH, 'ls', out_path, '--as=output', '--type=action'],
        check=True,
        capture_output=True,
        text=True)
    all_actions_process.check_returncode()
    build_script_actions = [
        action for action in all_actions_process.stdout.split("\n")
        # Skip roboelectric actions.
        if "build_script_output" in action and "robolectric" not in action
    ]
    # Split the build script actions into chunk of _MAX_TARGETS_PER_NINJA_EXECUTION.
    # This is needed in order not to exceed the command-line length.
    build_script_actions_chunk = [
        build_script_actions[i:i + _MAX_TARGETS_PER_NINJA_EXECUTION] for i in
        range(0, len(build_script_actions), _MAX_TARGETS_PER_NINJA_EXECUTION)
    ]
    for chunk in build_script_actions_chunk:
        if cronet_utils.build_all(out_path, chunk) != 0:
            raise Exception(
                f"Failed to build the following build actions scripts chunk: {chunk}."
            )


def _get_target_name_from_file(file_name: str) -> str:
    """Extracts the target name which generated the file from
  the directory path.

  The file_name format is "[X]/gen/path/to/BUILDGN/target_name/file_name"

  [X] is clang_* if this is a host version of the target, otherwise
  [X] is an empty string.


  Args:
    file_name: Path to cargo_flags.rs file relative in GN output dir

  Returns:
    GN target label that generated the file.
  """
    # Remove everything before gen/ directory
    file_name = re.sub(".*gen\/", "", file_name)
    dirs = file_name.split("/")
    build_gn_path = "/".join(dirs[0:-2])
    target_name = dirs[-2]
    return f"//{build_gn_path}:{target_name}"


def _generate_build_script_outputs_for_host(
        targets: List[str]) -> Dict[str, List[str]]:
    return _generate_build_script_outputs_for_arch("x64", targets)


def _generate_build_script_outputs_for_arch(arch: str,
                                            host_variant: bool = False
                                            ) -> Dict[str, List[str]]:
    # gn desc behaves completely differently when the output
    # directory is outside of chromium/src, some paths will
    # stop having // in the beginning of their labels
    # eg (//A/B will become A/B), this mostly apply to files
    # that are generated through actions and not targets.
    # This is why the temporary directory has to be generated
    # beneath the repository root until gn2bp is tweaked to
    # deal with this small differences.
    target_name_to_build_script_output = {}
    with tempfile.TemporaryDirectory(dir=_OUT_DIR) as gn_out_dir:
        cronet_utils.gn(gn_out_dir, _get_args_for_arch(arch))
        _build_rust_build_script_actions(gn_out_dir)
        build_script_output_files = _find_all_host_cargo_flags_files(
            gn_out_dir) if host_variant else _find_all_cargo_flags_files(
                gn_out_dir)

        for build_script_output_file in build_script_output_files:
            target_name = _get_target_name_from_file(build_script_output_file)
            target_name_to_build_script_output[
                target_name] = cronet_utils.read_file(
                    os.path.join(
                        gn_out_dir,
                        build_script_output_file)).rstrip("\n").split("\n")
    return target_name_to_build_script_output


def _generate_build_scripts_outputs(
        archs: List[str],
        targets: List[str]) -> Dict[str, Dict[str, List[str]]]:
    build_scripts_output_per_arch = {}
    for arch in archs:
        build_script_output = _generate_build_script_outputs_for_arch(arch)
        for (target_name, output) in build_script_output.items():
            if targets and target_name not in targets:
                continue
            if target_name not in build_scripts_output_per_arch:
                build_scripts_output_per_arch[target_name] = {}
            build_scripts_output_per_arch[target_name][arch] = output

    # Generate host-specific build script outputs
    build_script_output = _generate_build_script_outputs_for_host(targets)
    for (target_name, output) in build_script_output.items():
        if targets and target_name not in targets:
            continue
        if target_name not in build_scripts_output_per_arch:
            build_scripts_output_per_arch[target_name] = {}
        build_scripts_output_per_arch[target_name]["host"] = output
    return build_scripts_output_per_arch


def dump_build_scripts_outputs_to_file(
        output_file_path: str,
        archs: List[str],
        targets_to_build: List[str] = None) -> None:
    """Dumps a JSON formatted string that maps from target
  name to build scripts output.

  Args:
    output_file_path: Path of the file to write the output to
    archs: List of archs to compile for
    targets_to_build: If specified, only those targets build_script will
    be present in the final output. Otherwise, everything will be available.
  """
    with open(output_file_path, "w") as output_file:
        output_file.write(
            json.dumps(_generate_build_scripts_outputs(archs,
                                                       targets_to_build),
                       indent=2,
                       sort_keys=True))


def main():
    parser = argparse.ArgumentParser(
        description=
        'Generates a JSON dictionary containing the mapping between GN target labels to Rust build script output'
    )
    parser.add_argument(
        '--output',
        type=str,
        help='Path to file for which the output will be written to',
        required=True)
    args = parser.parse_args()
    dump_build_scripts_outputs_to_file(args.output, _ARCHS)


if __name__ == '__main__':
    sys.exit(main())
