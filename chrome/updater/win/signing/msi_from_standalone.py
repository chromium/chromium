#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Builds an MSI around the supplied offline/standalone installer.

This MSI installer is intended to enable enterprise installation scenarios while
being as close to a normal install as possible.

This method only works for application installers that do not use MSI.

For example, to create `GoogleChromeBetaStandaloneEnterprise.msi` from
`ChromeBetaOfflineSetup.exe`:
```
python3 chrome/updater/win/signing/msi_from_standalone.py
    --candle_path ../third_party/wix/v3_8_1128/files/candle.exe
    --light_path ../third_party/wix/v3_8_1128/files/light.exe
    --product_name "Google Chrome Beta"
    --product_version 110.0.5478.0
    --appid {8237E44A-0054-442C-B6B6-EA0509993955}
    --product_custom_params "&brand=GCEA"
    --product_uninstaller_additional_args=--force-uninstall
    --product_installer_data "%7B%22dis%22%3A%7B%22msi%22%3Atrue%7D%7D"
    --standalone_installer_path ChromeBetaOfflineSetup.exe
    --custom_action_dll_path out/Default/msi_custom_action.dll
    --msi_base_name GoogleChromeBetaStandaloneEnterprise
    --enterprise_installer_dir chrome/updater/win/signing
    --company_name "Google"
    --company_full_name "Google LLC"
    --output_dir out/Default
```

"""

import binascii
from datetime import date
import hashlib
import argparse
import os
import shutil
import subprocess

_GOOGLE_UPDATE_NAMESPACE_GUID = 'BE19B3E4502845af8B3E67A99FCDCFB1'


def convert_to_msi_version_number_if_needed(product_version):
    """Change product_version to fit in an MSI version number if needed.

  Some products use a 4-field version numbering scheme whereas MSI looks only
  at the first three fields when considering version numbers. Furthermore, MSI
  version fields have documented width restrictions of 8bits.8bits.16bits as
  per http://msdn.microsoft.com/en-us/library/aa370859(VS.85).aspx

  As such, the following scheme is used:

  Product a.b.c.d -> MSI X.Y.Z:
    X = (1 << 6) | ((C & 0xffff) >> 10)
    Y = (C >> 2) & 0xff
    Z = ((C & 0x3) << 14) | (D & 0x3FFF)

  So eg. 6.1.420.8 would become 64.105.8

  This assumes:
  1) we care about neither the product major number nor the product minor
     number, e.g. we will never reset the 'c' number after an increase in
     either 'a' or 'b'.
  2) 'd' will always be <= 16383
  3) 'c' is <= 65535

  We assert on assumptions 2) and 3)

  Args:
    product_version: A version string in "#.#.#.#" format.

  Returns:
    An MSI-compatible version string, or if product_version is not of the
    expected format, then the original product_version value.
  """

    try:
        version_field_strings = product_version.split('.')
        (build, patch) = [int(x) for x in version_field_strings[2:]]
    except:  # pylint: disable=bare-except
        # Couldn't parse the version number as a 4-term period-separated number,
        # just return the original string.
        return product_version

    # Check that the input version number is in range.
    assert patch <= 16383, 'Error, patch number %s out of range.' % patch
    assert build <= 65535, 'Error, build number %s out of range.' % build

    msi_major = (1 << 6) | ((build & 0xffff) >> 10)
    msi_minor = (build >> 2) & 0xff
    msi_build = ((build & 0x3) << 14) | (patch & 0x3FFF)

    return str(msi_major) + '.' + str(msi_minor) + '.' + str(msi_build)


def get_installer_namespace():
    return binascii.a2b_hex(_GOOGLE_UPDATE_NAMESPACE_GUID)


def generate_name_based_guid(namespace, name):
    """Generate a GUID based on the names supplied.

  Follows a methodology recommended in Section 4.3 of RFC 4122 to generate
  a "name-based UUID," which basically means that you want to control the
  inputs to the GUID so that you can generate the same valid GUID each time
  given the same inputs.

  Args:
    namespace: First part of identifier used to generate GUID
    name: Second part of identifier used to generate GUID

  Returns:
    String representation of the generated GUID.

  Raises:
    Nothing.
  """

    # Generate 128 unique bits.
    mymd5 = hashlib.md5()
    mymd5.update(namespace + name.encode('utf-8'))
    md5_hex_digest = mymd5.hexdigest()
    md5_hex_digits = [
        md5_hex_digest[x:x + 2].upper()
        for x in range(0, len(md5_hex_digest), 2)
    ]

    # Set various reserved bits to make this a valid GUID.

    # "Set the four most significant bits (bits 12 through 15) of the
    # time_hi_and_version field to the appropriate 4-bit version number
    # from Section 4.1.3."
    version = int(md5_hex_digits[6], 16)
    version = 0x30 | (version & 0x0f)

    # "Set the two most significant bits (bits 6 and 7) of the
    # clock_seq_hi_and_reserved to zero and one, respectively."
    clock_seq_hi_and_reserved = int(md5_hex_digits[8], 16)
    clock_seq_hi_and_reserved = 0x80 | (clock_seq_hi_and_reserved & 0x3f)

    return ('%s-%s-%02X%s-%02X%s-%s' %
            (''.join(md5_hex_digits[0:4]), ''.join(md5_hex_digits[4:6]),
             version, md5_hex_digits[7], clock_seq_hi_and_reserved,
             md5_hex_digits[9], ''.join(md5_hex_digits[10:])))


def optional_flag(key, value):
    return (f'-d{key}={value}', ) if value else ()


def get_wix_candle_flags(
        product_name,
        product_name_legal_identifier,
        msi_product_version,
        product_version,
        appid,
        company_name,
        company_full_name,
        custom_action_dll_path=None,
        product_uninstaller_additional_args=None,
        msi_product_id=None,
        msi_upgradecode_guid=None,
        product_installer_path=None,
        product_installer_data=None,
        product_icon_path=None,
        product_installer_install_command=None,
        product_installer_disable_update_registration_arg=None,
        product_custom_params=None,
        standalone_installer_path=None,
        metainstaller_path=None,
        architecture=None):
    """Generate the proper set of defines for WiX Candle usage."""
    flags = [
        *optional_flag('ProductName', product_name),
        *optional_flag('ProductNameLegalIdentifier',
                       product_name_legal_identifier),
        *optional_flag('ProductVersion', msi_product_version),
        *optional_flag('ProductOriginalVersionString', product_version),
        *optional_flag('ProductBuildYear', str(date.today().year)),
        *optional_flag('ProductGuid', appid),
        *optional_flag('CompanyName', company_name),
        *optional_flag('CompanyFullName', company_full_name),
        *optional_flag('MsiInstallerCADll', custom_action_dll_path),
        *optional_flag('ProductUninstallerAdditionalArgs',
                       product_uninstaller_additional_args),
        *optional_flag('MsiProductId', msi_product_id),
        *optional_flag('MsiUpgradeCode', msi_upgradecode_guid),
        *optional_flag('ProductCustomParams', "%s" % product_custom_params),
        *optional_flag('StandaloneInstallerPath', standalone_installer_path),
        *optional_flag('GoogleUpdateMetainstallerPath',
                       "%s" % metainstaller_path),
        *optional_flag('ProductInstallerInstallCommand',
                       product_installer_install_command),
        *optional_flag('ProductInstallerDisableUpdateRegistrationArg',
                       product_installer_disable_update_registration_arg),
        *optional_flag('ProductInstallerPath', product_installer_path),
        *optional_flag(
            'ProductInstallerData',
            product_installer_data.replace('==MSI-PRODUCT-ID==',
                                           msi_product_id)
            if product_installer_data else None),
        *optional_flag('ProductIcon', product_icon_path),
    ]

    if architecture:
        # Translate some common strings, like from platform.machine().
        arch_map = {
            'amd64': 'x64',
            'x86_64': 'x64',
        }
        flags.extend(['-arch', arch_map.get(architecture, architecture)])
    return flags


def get_wix_light_flags():
    # Disable warning LGHT1076 and internal check ICE61 on light.exe.
    return ['-sw1076', '-sice:ICE61']


class EnterpriseInstaller:
    """Creates an enterprise installer from a standalone installer."""
    def __init__(self, candle_path, light_path, product_name, product_version,
                 appid, product_custom_params,
                 product_uninstaller_additional_args, product_installer_data,
                 standalone_installer_path, custom_action_dll_path,
                 msi_base_name, enterprise_installer_dir, company_name,
                 company_full_name, output_dir):
        self._candle_path = candle_path
        self._light_path = light_path
        self._product_name = product_name
        self._product_version = product_version
        self._appid = appid
        self._product_custom_params = product_custom_params
        self._product_uninstaller_additional_args = (
            product_uninstaller_additional_args)
        self._product_installer_data = product_installer_data
        self._standalone_installer_path = standalone_installer_path
        self._custom_action_dll_path = custom_action_dll_path
        self._msi_base_name = msi_base_name
        self._enterprise_installer_dir = enterprise_installer_dir
        self._company_name = company_name
        self._company_full_name = company_full_name
        self._output_dir = output_dir

    def BuildInstaller(self):
        product_name_legal_identifier = self._product_name.replace(' ', '')
        msi_name = self._msi_base_name + '.msi'
        msi_product_version = convert_to_msi_version_number_if_needed(
            self._product_version)

        updater_installer_namespace = get_installer_namespace()

        # Include the .msi filename in the Product Code generation because "the
        # product code must be changed if... the name of the .msi file has been
        # changed" according to msdn.microsoft.com/en-us/library/aa367850.aspx.
        # Also include the version number since we process version changes as
        # major upgrades.
        msi_product_id = generate_name_based_guid(
            updater_installer_namespace, 'Product %s %s %s' %
            (self._product_name, self._msi_base_name, self._product_version))
        msi_upgradecode_guid = generate_name_based_guid(
            updater_installer_namespace, 'Upgrade ' + self._product_name)

        # To allow for multiple versions of the same product to be generated,
        # stick output in a subdirectory.
        output_directory_name = os.path.join(
            self._output_dir, self._appid + '.' + self._product_version)
        if not os.path.exists(output_directory_name):
            os.makedirs(output_directory_name)

        target_wxs = os.path.join(output_directory_name,
                                  self._msi_base_name + '.wxs')
        shutil.copyfile(
            os.path.join(self._enterprise_installer_dir,
                         'enterprise_standalone_installer.wxs.xml'),
            target_wxs)

        wix_candle_flags = get_wix_candle_flags(
            self._product_name,
            product_name_legal_identifier,
            msi_product_version,
            self._product_version,
            self._appid,
            self._company_name,
            self._company_full_name,
            product_custom_params=self._product_custom_params,
            standalone_installer_path=self._standalone_installer_path,
            custom_action_dll_path=self._custom_action_dll_path,
            product_uninstaller_additional_args=self.
            _product_uninstaller_additional_args,
            msi_product_id=msi_product_id,
            msi_upgradecode_guid=msi_upgradecode_guid,
            product_installer_data=self._product_installer_data)

        wix_light_flags = get_wix_light_flags()
        msi_output_path = os.path.join(output_directory_name, msi_name)
        msi_output_path_wixobj = os.path.splitext(
            msi_output_path)[0] + '.wixobj'

        candle_cmd = [self._candle_path] + wix_candle_flags + [
            '-nologo', '-o', msi_output_path_wixobj, target_wxs
        ]
        subprocess.run(candle_cmd, check=True)
        light_cmd = [self._light_path] + wix_light_flags + [
            '-nologo', '-out', msi_output_path, msi_output_path_wixobj
        ]
        subprocess.run(light_cmd, check=True)


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
    parser.add_argument('--product_custom_params',
                        required=True,
                        help='custom values to be appended to the updater tag')
    parser.add_argument(
        '--product_uninstaller_additional_args',
        required=True,
        help=(
            'extra command line parameters that the custom action dll will '
            'pass on to the product uninstaller, typically you will want to '
            'pass any extra arguments that will force the uninstaller to run '
            'silently here.'))
    parser.add_argument(
        '--product_installer_data',
        required=True,
        help=(
            'installer data to be passed to the product installer at run '
            'time, url-encoded. This is needed since command line parameters '
            'cannot be passed to the product installer when it is wrapped in '
            'a standalone installer.'))
    parser.add_argument('--standalone_installer_path',
                        required=True,
                        help='path to product standalone installer')
    parser.add_argument(
        '--custom_action_dll_path',
        required=True,
        help=(
            'path to the custom action dll that exports '
            '`ShowInstallerResultUIString` and `ExtractTagInfoFromInstaller` '
            'methods. ShowInstallerResultUIString reads the '
            'LastInstallerResultUIString from the product ClientState key in '
            'the registry and display the string via MsiProcessMessage. '
            'ExtractTagInfoFromInstaller extracts brand code from tagged MSI '
            'package.'))
    parser.add_argument('--msi_base_name',
                        required=True,
                        help='root of name for the MSI')
    parser.add_argument(
        '--enterprise_installer_dir',
        required=True,
        help=
        'path to dir which contains enterprise_standalone_installer.wxs.xml')
    parser.add_argument('--company_name',
                        default='Google',
                        help='company name for the application')
    parser.add_argument('--company_full_name',
                        default='Google LLC',
                        help='company full name for the application')
    parser.add_argument(
        '--output_dir',
        required=True,
        help='path to the directory that will contain the resulting MSI')
    args = parser.parse_args()
    EnterpriseInstaller(
        args.candle_path, args.light_path, args.product_name,
        args.product_version, args.appid, args.product_custom_params,
        args.product_uninstaller_additional_args, args.product_installer_data,
        args.standalone_installer_path, args.custom_action_dll_path,
        args.msi_base_name, args.enterprise_installer_dir, args.company_name,
        args.company_full_name, args.output_dir).BuildInstaller()


if __name__ == '__main__':
    main()
