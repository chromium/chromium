#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Builds a test MSI with the supplied parameters.

For example, to create `TestMsiInstaller.msi`:
```
python3 chrome/updater/test/test_installer/create_test_msi_installer.py
  --candle_path third_party/wix/candle.exe
  --light_path third_party/wix/light.exe
  --product_name "Test MSI Installer"
  --product_version 1.0.0.0
  --appid {8A898FC4-7309-4D27-8BC8-7A5152227458}
  --msi_base_name TestMsiInstaller
  --msi_template_path chrome/updater/test/test_installer/test_installer.wxs.xml
  --per_user_install
  --output_dir out/Default
```

"""

import binascii
from datetime import date
import argparse
import filecmp
import os
import shutil
import subprocess
import sys
import tempfile
import uuid


def optional_flag(key, value):
    return (f'-d{key}={value}', ) if value else ()


class TestInstaller:
    """Creates a test installer."""
    def __init__(self, candle_path, light_path, product_name, product_version,
                 appid, msi_base_name, msi_template_path, company_name,
                 company_full_name, per_user_install, checked_in_msi,
                 output_dir):
        self._candle_path = candle_path
        self._light_path = light_path
        self._product_name = product_name
        self._product_version = product_version
        self._appid = appid
        self._msi_base_name = msi_base_name
        self._msi_template_path = msi_template_path
        self._company_name = company_name
        self._company_full_name = company_full_name
        self._per_user_install = per_user_install
        self._checked_in_msi = checked_in_msi
        self._output_dir = output_dir

    def BuildInstaller(self):
        output_directory_name = os.path.join(
            self._output_dir, self._appid + '.' + self._product_version)
        if not os.path.exists(output_directory_name):
            os.makedirs(output_directory_name)
        msi_output_path = os.path.join(output_directory_name,
                                       self._msi_base_name + '.msi')
        msi_base_file_path = os.path.splitext(self._checked_in_msi)[0]
        target_wxs = msi_base_file_path + '.wxs'

        if sys.platform == 'win32' and os.path.isfile(
                self._candle_path) and os.path.isfile(self._light_path) and (
                    not os.path.isfile(target_wxs) or not filecmp.cmp(
                        self._msi_template_path, target_wxs, shallow=False)):
            checked_in_dir = os.path.dirname(self._checked_in_msi)
            if not os.path.exists(checked_in_dir):
                os.makedirs(checked_in_dir)

            test_installer_namespace = '{A2091DEA-AF86-4C00-8AE0-ECC38FDE6533}'
            namespace_uuid = uuid.UUID(test_installer_namespace)
            names_plus_version = '%s %s %s' % (
                self._product_name, self._msi_base_name, self._product_version)

            wix_candle_flags = [
                *optional_flag('ProductName', self._product_name),
                *optional_flag('ProductNameLegalIdentifier',
                               self._product_name.replace(' ', '')),
                *optional_flag('ProductVersion', self._product_version),
                *optional_flag('ProductOriginalVersionString',
                               self._product_version),
                *optional_flag('ProductBuildYear', str(date.today().year)),
                *optional_flag('ProductGuid', self._appid),
                *optional_flag('CompanyName', self._company_name),
                *optional_flag('CompanyFullName', self._company_full_name),
                *optional_flag('PerUserInstall', self._per_user_install),
                *optional_flag(
                    'MsiProductId',
                    str(
                        uuid.uuid5(namespace_uuid, 'Product %s' %
                                   names_plus_version)).upper()),
                *optional_flag(
                    'MsiUpgradeCode',
                    str(
                        uuid.uuid5(namespace_uuid,
                                   'Upgrade ' + self._product_name)).upper()),
                *optional_flag(
                    'ComponentGuidInstallerResultSet',
                    str(
                        uuid.uuid5(
                            namespace_uuid,
                            'Component InstallerResult Set %s' %
                            names_plus_version)).upper()),
                *optional_flag(
                    'ComponentGuidInstallerErrorSet',
                    str(
                        uuid.uuid5(
                            namespace_uuid, 'Component InstallerError Set %s' %
                            names_plus_version)).upper()),
                *optional_flag(
                    'ComponentGuidInstallerResultUIStringSet',
                    str(
                        uuid.uuid5(
                            namespace_uuid,
                            'Component InstallerResultUIString Set %s' %
                            names_plus_version)).upper()),
                *optional_flag(
                    'ComponentGuidRegisterLaunchCommandSet',
                    str(
                        uuid.uuid5(
                            namespace_uuid,
                            'Component RegisterLaunchCommand Set %s' %
                            names_plus_version)).upper()),
            ]

            # Disable warning LGHT1076 and internal check ICE61 on light.exe.
            wix_light_flags = ['-sw1076', '-sice:ICE61']

            shutil.copyfile(self._msi_template_path, target_wxs)
            with tempfile.TemporaryDirectory() as temp_dir:
                msi_wixobj = os.path.join(temp_dir,
                                          self._msi_base_name + '.wixobj')
                msi_pdb = os.path.join(temp_dir,
                                       self._msi_base_name + '.wixpdb')
                candle_cmd = [self._candle_path] + wix_candle_flags + [
                    '-nologo', '-o', msi_wixobj, target_wxs
                ]
                subprocess.run(candle_cmd, check=True)
                light_cmd = [self._light_path] + wix_light_flags + [
                    '-nologo', '-pdbout', msi_pdb, '-out',
                    self._checked_in_msi, msi_wixobj
                ]
                subprocess.run(light_cmd, check=True)

        # Copy the checked-in files to the final output path.
        shutil.copyfile(self._checked_in_msi, msi_output_path)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--candle_path',
                        required=True,
                        help='path to the WiX candle.exe')
    parser.add_argument('--light_path',
                        required=True,
                        help='path to the WiX light.exe')
    parser.add_argument('--product_name',
                        required=True,
                        help='name of the product being built')
    parser.add_argument('--product_version',
                        required=True,
                        help='product version to be installed')
    parser.add_argument('--appid',
                        required=True,
                        help='updater application ID for product')
    parser.add_argument('--msi_base_name',
                        required=True,
                        help='root of name for the MSI')
    parser.add_argument('--msi_template_path',
                        required=True,
                        help='path to `test_installer.wxs.xml`')
    parser.add_argument('--company_name',
                        default='Google',
                        help='company name for the application')
    parser.add_argument('--company_full_name',
                        default='Google LLC',
                        help='company full name for the application')
    parser.add_argument('--per_user_install',
                        action='store_true',
                        help='specifies that the MSI is a per-user installer')
    parser.add_argument(
        '--checked_in_msi',
        required=True,
        help='specifies the location where the MSI will live in the source tree'
    )
    parser.add_argument(
        '--output_dir',
        required=True,
        help='path to the directory that will contain the resulting MSI')
    args = parser.parse_args()
    TestInstaller(args.candle_path, args.light_path, args.product_name,
                  args.product_version, args.appid, args.msi_base_name,
                  args.msi_template_path, args.company_name,
                  args.company_full_name, args.per_user_install,
                  args.checked_in_msi, args.output_dir).BuildInstaller()


if __name__ == '__main__':
    main()
