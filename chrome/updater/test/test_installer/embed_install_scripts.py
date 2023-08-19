#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# [VPYTHON:BEGIN]
# python_version: "3.8"
# wheel: <
#   name: "infra/python/wheels/pywin32/${vpython_platform}"
#    version: "version:300"
# >
# [VPYTHON:END]
"""An utility to embed setup scripts as the installer resources.

The scripts are embedded as resources at "SCRIPT\\BATCH" or "SCRIPT\\POWERSHELL"
or "SCRIPT\\PYTHON" based on the scripts types (file extensions).

Example:
vpython3 embed_install_scripts.py --installer test_installer.exe \
    --output app1_installer.exe --script app1_setup.cmd  --script more_setup.py

"""

import argparse
from typing import List
import os

import resedit

_EXTENSION_TO_TYPE = {
    '.bat': 'BATCH',
    '.cmd': 'BATCH',
    '.ps': 'POWERSHELL',
    '.py': 'PYTHON',
}


def script_type_from_path(script_path: str) -> str:
    _, extension = os.path.splitext(script_path.lower())
    return _EXTENSION_TO_TYPE[extension]


def embed_script_into_app_installer(installer_path: str, output_path: str,
                                    script_files: List[str]):
    resed = resedit.ResourceEditor(installer_path, output_path)
    english_language_id = 1033
    for script_file in script_files:
        script_type = script_type_from_path(script_file)
        resed.UpdateResource('SCRIPT', english_language_id, script_type,
                             script_file)
    resed.Commit()


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--installer',
                        required=True,
                        help='The path to the installer.')
    parser.add_argument(
        '--output',
        required=True,
        help=('The path to save the updated installer with the '
              'embedded scripts.'))
    parser.add_argument('--script',
                        action='append',
                        required=True,
                        help=('The path to the script to embed, supports '
                              'powershell, DOS batch file and python scripts, '
                              'at most one per each type.'))
    args = parser.parse_args()
    embed_script_into_app_installer(args.installer, args.output, args.script)


if __name__ == '__main__':
    main()
