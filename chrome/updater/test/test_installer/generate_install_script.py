#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import stat


def generate_app_install_script(command: str, app: str, company: str,
                                version: str, output: str):
    with open(output, "wt") as f:
        f.write('#/bin/bash\n\n')
        f.write('SCRIPT_DIR=$(dirname -- "${BASH_SOURCE[0]}")\n')
        f.write('"${SCRIPT_DIR}/' +
                f'{command}" --appid={app} --company={company} '
                f'--product_version={version}\n')
    os.chmod(output, os.stat(output).st_mode | stat.S_IEXEC)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--command', required=True, help='The command to run.')
    parser.add_argument('--output',
                        required=True,
                        help='The script output path.')
    parser.add_argument('--app', required=True, help='App to install.')
    parser.add_argument('--company',
                        required=False,
                        default='Chromium',
                        help='Owner company of the app.')
    parser.add_argument('--version', required=True, help='Version of the app.')
    args = parser.parse_args()
    generate_app_install_script(args.command, args.app, args.company,
                                args.version, args.output)


if __name__ == '__main__':
    main()
