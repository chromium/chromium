#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys
import tempfile
import pathlib
import argparse
from typing import List, Set

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

sys.path.insert(0, REPOSITORY_ROOT)
import components.cronet.tools.utils as cronet_utils  # pylint: disable=wrong-import-position
import build.android.gyp.util.build_utils as build_utils  # pylint: disable=wrong-import-position

_ARCHS = ["x86", "x64", "arm", "arm64"]
_GN2BP_SCRIPT_PATH = os.path.join(REPOSITORY_ROOT,
                                  "components/cronet/gn2bp/gen_android_bp.py")
_GENERATE_BUILD_SCRIPT_PATH = os.path.join(
    REPOSITORY_ROOT,
    "components/cronet/gn2bp/generate_build_scripts_output.py")
_OUT_DIR = os.path.join(REPOSITORY_ROOT, "out")
_EXTRA_GN_ARGS = ("is_cronet_for_aosp_build=true"),
_GN_PATH = os.path.join(REPOSITORY_ROOT, 'buildtools/linux64/gn')


def _run_gn2bp(desc_files: Set[tempfile.NamedTemporaryFile],
               skip_build_scripts: bool) -> int:
    with tempfile.NamedTemporaryFile(mode="w+",
                                     encoding='utf-8') as build_script_output:

        if skip_build_scripts:
            pathlib.Path(build_script_output.name).write_text("{}")
        elif _run_generate_build_scripts(build_script_output.name) != 0:
            raise ValueError("Failed to generate build scripts output!")

        base_cmd = [
            sys.executable, _GN2BP_SCRIPT_PATH, "--repo_root", REPOSITORY_ROOT,
            "--build_script_output", build_script_output.name
        ]
        for desc_file in desc_files:
            # desc_file.name represents the absolute path.
            base_cmd += ["--desc", desc_file.name]
        return cronet_utils.run(base_cmd)


def _run_generate_build_scripts(output_path: str) -> int:
    """Runs the generate_build_scripts_output.py

    Args:
      output_path: Path of the file that will contain the output.
    """
    return cronet_utils.run([
        sys.executable,
        _GENERATE_BUILD_SCRIPT_PATH,
        "--output",
        output_path,
    ])


def _get_args_for_aosp(arch: str) -> List[str]:
    default_args = cronet_utils.get_android_gn_args(True, arch)
    default_args += _EXTRA_GN_ARGS
    return ' '.join(
        cronet_utils.filter_gn_args(default_args,
                                    ["use_remoteexec", "enable_rust"]))


def _write_desc_json(gn_out_dir: str,
                     temp_file: tempfile.NamedTemporaryFile) -> int:
    return cronet_utils.run([
        _GN_PATH, "desc", gn_out_dir, "--format=json", "--all-toolchains",
        "//*"
    ],
                            stdout=temp_file)


def _main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--stamp',
                        type=str,
                        help='Path to touch on success',
                        required=True)
    parser.add_argument(
        '--skip_build_scripts',
        type=bool,
        help=
        'Skips building the build_scripts output, this should be only used for testing.'
    )
    args = parser.parse_args()
    try:
        # Create empty temp file for each architecture.
        arch_to_temp_desc_file = {
            arch: tempfile.NamedTemporaryFile(mode="w+", encoding='utf-8')
            for arch in _ARCHS
        }

        for (arch, temp_file) in arch_to_temp_desc_file.items():
            # gn desc behaves completely differently when the output
            # directory is outside of chromium/src, some paths will
            # stop having // in the beginning of their labels
            # eg (//A/B will become A/B), this mostly apply to files
            # that are generated through actions and not targets.
            #
            # This is why the temporary directory has to be generated
            # beneath the repository root until gn2bp is tweaked to
            # deal with this small differences.
            with tempfile.TemporaryDirectory(dir=_OUT_DIR) as gn_out_dir:
                cronet_utils.gn(gn_out_dir, _get_args_for_aosp(arch))
                if _write_desc_json(gn_out_dir, temp_file) != 0:
                    # Close the files and exit if we failed to generate any
                    # of the desc.json files.
                    print(f"Failed to generate desc file for arch: {arch}")
                    for file in arch_to_temp_desc_file.values():
                        # Close the temporary files so they can be deleted.
                        file.close()
                    sys.exit(-1)

        res = _run_gn2bp(arch_to_temp_desc_file.values(),
                         args.skip_build_scripts)
    finally:
        for file in arch_to_temp_desc_file.values():
            # Close the temporary files so they can be deleted.
            file.close()

    if res != 0:
        print("Failed to execute gn2bp!")
        sys.exit(-1)
    else:
        build_utils.Touch(args.stamp)
    return 0


if __name__ == '__main__':
    sys.exit(_main())
