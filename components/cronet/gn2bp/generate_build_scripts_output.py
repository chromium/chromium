# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Rust crates can generate code at build time through a host-compiled Rust binary called a "build script".

These build scripts can generate two kinds of artifacts:

* cargo_flags.rs, which is a set of rustc compiler options that should be used when
building the crate;
* Arbitrary Rust code to be used by the crate.

Android.bp does not support build scripts. In gn2bp we work around this by generating
the above on the Chromium side prior to running gen_android_bp; As seen in _generate_build_scripts_outputs.
For cargo_flags.rs, This script outputs a file containing  the per-GN-target-per-architecture rustc flags
to use, and this is consumed by gn_utils. As for the rest (arbitrary generated Rust code), it is handled in
gen_android_bp.py#create_modules_from_target.
"""
import argparse
from typing import Set, List, Dict
import glob
import multiprocessing.dummy
import json
import tempfile
import re
import subprocess
import os
import sys
import json
import shutil

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))
sys.path.insert(0, REPOSITORY_ROOT)

import components.cronet.tools.utils as cronet_utils  # pylint: disable=wrong-import-position
import components.cronet.gn2bp.common as gn2bp_common  # pylint: disable=wrong-import-position
import components.cronet.gn2bp.gen_android_bp as cronet_gn2bp  # pylint: disable=wrong-import-position

# TODO: Move ARCHS to cronet_utils.
_ARCHS = ["x86", "x64", "arm", "arm64", "riscv64"]
# TODO: Move _OUT_DIR to cronet_utils.
_OUT_DIR = os.path.join(REPOSITORY_ROOT, "out")


def _extract_crate_path(args: List[str]) -> str:
  """Extracts the path where the crate actually exist from the args of a build script
  action.

  Args:
    args: Build script GN action arguments

  Returns:
    The path to the rust crate relative to Chromium repository.
  """
  return args[args.index("--src-dir") + 1].replace("../../", "")


def _get_toolchain_name(toolchain_label: str) -> str:
  return toolchain_label[toolchain_label.find(":") + 1:]


def _get_toolchain_label_from_label(target_label: str) -> str:
  return target_label[target_label.find('(') + 1:-1]


def _get_label_name_without_toolchain(target_label: str) -> str:
  return target_label[:target_label.find("(")]


def _build_rust_build_script_actions(
    out_path: str, host_variant: bool) -> Dict[str, Dict[str, any]]:
  targets_data = json.loads(
      cronet_utils.run_and_get_stdout([
          cronet_utils.GN_PATH, "desc", out_path, "//*", "--format=json",
          "--type=action"
      ]))
  possible_candidates = {}
  for target_name, target_data in targets_data.items():
    if gn2bp_common.is_rust_build_script(target_data.get("script", "")):
      # "clang_x64" assumes that the host uses clang to compile for the host machine.
      # which is mostly true, unless gcc is used which is a case that we don't care about.
      if host_variant and _get_toolchain_name(
          target_data['toolchain']).startswith("clang_x64"):
        possible_candidates[target_name[2:]] = target_data
      elif not host_variant and _get_toolchain_name(
          target_data['toolchain']).startswith("android_clang_"):
        possible_candidates[target_name[2:]] = target_data

  # The generated Ninja targets do not support the "//...(//toolchain)` syntax. So to workaround
  # this limitation, build the phony target which matches the host-toolchain target. A target whose
  # name is "A/B/C:D(//path/to/toolchain:toolchain_name)" will be mapped to
  # "toolchain_name/phony/A/B/C/D".
  cronet_utils.build_targets_list_chunking(out_path, [
      name if not host_variant else
      f'{_get_toolchain_name(_get_toolchain_label_from_label(name))}/phony/{_get_label_name_without_toolchain(name.replace(":", "/"))}'
      for name in possible_candidates
  ])
  return possible_candidates

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


def _generate_and_copy_build_script_outputs_for_host() -> Dict[str, any]:
  return _generate_and_copy_build_script_outputs_for_arch(arch="x64",
                                                          host_variant=True)


def _generate_and_copy_build_script_outputs_for_arch(arch: str,
                                                     host_variant: bool = False
                                                     ) -> Dict[str, any]:
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
    cronet_utils.gn(gn_out_dir,
                    ' '.join(cronet_utils.get_gn_args_for_aosp(arch)))
    candidate_targets = _build_rust_build_script_actions(
        gn_out_dir, host_variant=host_variant)
    for target_data in candidate_targets.values():
      output_files = [
          '/'.join(file_name.removeprefix("//").split("/")[2:])
          for file_name in target_data["outputs"] if file_name.endswith(".rs")
      ]

      cargo_rs = next(file for file in output_files
                      if file.endswith("cargo_flags.rs"))
      target_name_to_build_script_output[_get_target_name_from_file(
          cargo_rs)] = cronet_utils.read_file(os.path.join(
              gn_out_dir, cargo_rs)).rstrip("\n").split("\n")
      for output_file in output_files:
        output_path = os.path.join(
            REPOSITORY_ROOT,
            _extract_crate_path(target_data['args']),
            "gn2bp_rust_build_script_outputs",
            (arch if not host_variant else "host"),
            # TODO(crbug.com/448059753): Support outputs nested in a sub-directory.
            output_file.split("/")[-1])
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        shutil.copy(os.path.join(gn_out_dir, output_file), output_path)
  return target_name_to_build_script_output


def _generate_build_scripts_outputs(
    archs: List[str],
    targets: List[str] = None) -> Dict[str, Dict[str, List[str]]]:
  build_scripts_output_per_arch = {}
  with multiprocessing.dummy.Pool(len(archs)) as pool:
    results = [
        (arch,
         pool.apply_async(_generate_and_copy_build_script_outputs_for_arch,
                          (arch, False))) for arch in archs
    ]
    for (arch, result) in results:
      build_script_output = result.get()
      for (target_name, output) in build_script_output.items():
        if targets and target_name not in targets:
          continue
        if target_name not in build_scripts_output_per_arch:
          build_scripts_output_per_arch[target_name] = {}
        build_scripts_output_per_arch[target_name][arch] = output

  # Generate host-specific build script outputs
  build_script_output = _generate_and_copy_build_script_outputs_for_host()
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
        json.dumps(_generate_build_scripts_outputs(archs, targets_to_build),
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
