#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Example of setup script that writes the app 'pv' value into the registry."""

import argparse
import sys
import winreg


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--company',
                        default='Google',
                        required=False,
                        help='App ID.')
    parser.add_argument('--appid',
                        default='{AE098195-B8DB-4A49-8E23-84FCACB61FF1}',
                        required=False,
                        help='App ID.')
    parser.add_argument('--system',
                        default=False,
                        required=False,
                        action='store_true',
                        help='Specify that this is a system install.')
    parser.add_argument('--product_version',
                        default='1.0.0.0',
                        required=False,
                        help='App version to install.')

    args = parser.parse_args()

    try:
        reg_hive = winreg.ConnectRegistry(
            None, winreg.HKEY_LOCAL_MACHINE
            if args.system else winreg.HKEY_CURRENT_USER)
        update_clients_key = winreg.CreateKeyEx(
            reg_hive,
            'Software\\%s\\Update\\Clients\\%s' % (args.company, args.appid),
            access=winreg.KEY_WOW64_32KEY | winreg.KEY_WRITE)
        winreg.SetValueEx(update_clients_key, 'pv', 0, winreg.REG_SZ,
                          args.product_version)
    except WindowsError:
        sys.exit(1)

    sys.exit(0)


if __name__ == '__main__':
    main()
