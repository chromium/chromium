# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Generates the rust build script output into a well-structured JSON format.
"""
import argparse
from typing import Set, List
import glob
import json
import tempfile
import re
import os
import sys

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))
sys.path.insert(0, REPOSITORY_ROOT)

import components.cronet.tools.utils as cronet_utils  # pylint: disable=wrong-import-position
import components.cronet.gn2bp.gen_android_bp as cronet_gn2bp  # pylint: disable=wrong-import-position
# Default targets to compile before crawling the output directory
# for build scripts output. It is important to note that this list
# should be consistent with the list in GN2BP

_TARGETS = cronet_gn2bp.DEFAULT_TARGETS + cronet_gn2bp.DEFAULT_TESTS + [
    '//build/rust/tests:tests',
]

# TODO: Move ARCHS to cronet_utils.
_ARCHS = ["x86", "x64", "arm", "arm64", "riscv64"]
# TODO: Move _OUT_DIR to cronet_utils.
_OUT_DIR = os.path.join(REPOSITORY_ROOT, "out")


def _find_all_cargo_flags_files(out_dir: str) -> Set[str]:
    return set(
        glob.glob("gen/**/cargo_flags.rs", root_dir=out_dir, recursive=True))


def _find_all_host_cargo_flags_files(out_dir: str) -> Set[str]:
    return set(
        glob.glob("clang_*/gen/**/cargo_flags.rs",
                  root_dir=out_dir,
                  recursive=True))


def _get_args_for_arch(arch: str) -> List[str]:
    default_args = cronet_utils.get_android_gn_args(True, arch)
    return ' '.join(
        cronet_utils.filter_gn_args(default_args,
                                    ["use_remoteexec", "enable_rust"]))


def _build_cronet_targets(out_path: str, targets: List[str]):
    if cronet_utils.build_all(
            out_path, [target.removeprefix("//") for target in targets]) != 0:
        raise Exception(
            f"Failed to build the provided targets {targets}. Check the logs for more information"
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
        targets: List[str]) -> dict[str, List[str]]:
    return _generate_build_script_outputs_for_arch("x64", targets, True)


def _generate_build_script_outputs_for_arch(
        arch: str,
        targets: List[str],
        host_variant: bool = False) -> dict[str, List[str]]:
    # gn desc behaves completely differently when the output
    # directory is outside of chromium/src, some paths will
    # stop having // in the beginning of their labels
    # eg (//A/B will become A/B), this mostly apply to files
    # that are generated through actions and not targets.
    # This is why the temporary directory has to be generated
    # beneath the repository root until gn2bp is tweaked to
    # deal with this small differences.
    target_name_to_build_script_output = {}
    with tempfile.TemporaryDirectory(dir=_OUT_DIR,
                                     ignore_cleanup_errors=True) as gn_out_dir:
        cronet_utils.gn(gn_out_dir, _get_args_for_arch(arch))
        _build_cronet_targets(gn_out_dir, targets)
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
        archs: List[str], targets: List[str],
        fetch_transitive_outputs: bool) -> dict[str, dict[str, List[str]]]:
    build_scripts_output_per_arch = {}
    for arch in archs:
        build_script_output = _generate_build_script_outputs_for_arch(
            arch, targets)
        for (target_name, output) in build_script_output.items():
            if not fetch_transitive_outputs and target_name not in targets:
                continue
            if target_name not in build_scripts_output_per_arch:
                build_scripts_output_per_arch[target_name] = {}
            build_scripts_output_per_arch[target_name][arch] = output

    # Generate host-specific build script outputs
    build_script_output = _generate_build_script_outputs_for_host(targets)
    for (target_name, output) in build_script_output.items():
        if not fetch_transitive_outputs and target_name not in targets:
            continue
        if target_name not in build_scripts_output_per_arch:
            build_scripts_output_per_arch[target_name] = {}
        build_scripts_output_per_arch[target_name]["host"] = output
    return build_scripts_output_per_arch


def dump_build_scripts_outputs_to_file(
        output_file_path: str,
        archs: List[str],
        targets_to_build: List[str],
        fetch_transitive_outputs: bool = True) -> None:
    """Dumps a JSON formatted string that maps from target
  name to build scripts output.

  Args:
    output_file_path: Path of the file to write the output to
    archs: List of archs to compile for
    targets_to_build: Targets which will be compiled to get their
    output
    fetch_transitive_outputs: Determines if the build script output
    of transitive dependencies should be also dumped, otherwise only the
    declared targets will exist in the final output. Defaults to True.
  """
    with open(output_file_path, "w") as output_file:
        output_file.write(
            json.dumps(_generate_build_scripts_outputs(
                archs, targets_to_build, fetch_transitive_outputs),
                       indent=2))


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
    dump_build_scripts_outputs_to_file(args.output, _ARCHS, _TARGETS)


if __name__ == '__main__':
    sys.exit(main())
