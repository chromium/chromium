# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import hashlib
import os
import string
import sys
import win32api
import win32com.client
from win32com.shell import shell, shellcon
import win32security

sys.path.insert(
    0,
    os.path.join(os.path.dirname(__file__), '..', '..', '..', 'third_party',
                 'pefile_py3'))
import pefile


def _GetFileVersion(file_path):
    """Returns the file version of the given file."""
    return win32com.client.Dispatch(
        'Scripting.FileSystemObject').GetFileVersion(file_path)


def _GetFileBitness(file_path):
    """Returns the bitness of the given file."""
    processor_type = pefile.PE(file_path).FILE_HEADER.Machine
    if processor_type == pefile.MACHINE_TYPE['IMAGE_FILE_MACHINE_I386']:
        return '32'
    if processor_type in [
            pefile.MACHINE_TYPE['IMAGE_FILE_MACHINE_AMD64'],
            pefile.MACHINE_TYPE['IMAGE_FILE_MACHINE_ARM64']
    ]:
        return '64'
    raise Exception('Unknown processor type %d' % processor_type)


def _GetProductName(file_path):
    """Returns the product name of the given file.

    Args:
        file_path: The absolute or relative path to the file.

    Returns:
        A string representing the product name of the file, or None if the
        product name was not found.
    """
    language_and_codepage_pairs = win32api.GetFileVersionInfo(
        file_path, '\\VarFileInfo\\Translation')
    if not language_and_codepage_pairs:
        return None
    product_name_entry = ('\\StringFileInfo\\%04x%04x\\ProductName' %
                          language_and_codepage_pairs[0])
    return win32api.GetFileVersionInfo(file_path, product_name_entry)


def _GetUserSpecificRegistrySuffix():
    """Returns '.' + the unpadded Base32 encoding of the MD5 of the user's SID.

    The result must match the output from the method
    UserSpecificRegistrySuffix::GetSuffix() in
    chrome/installer/util/shell_util.cc. It will always be 27 characters long.
    """
    token_handle = win32security.OpenProcessToken(win32api.GetCurrentProcess(),
                                                  win32security.TOKEN_QUERY)
    user_sid, _ = win32security.GetTokenInformation(token_handle,
                                                    win32security.TokenUser)
    user_sid_string = win32security.ConvertSidToStringSid(user_sid)
    md5_digest = hashlib.md5(user_sid_string.encode('utf-8')).digest()
    return '.' + base64.b32encode(md5_digest).decode('utf-8').rstrip('=')


class VariableExpander:
    """Expands variables in strings."""

    def __init__(self, mini_installer_path,
                 previous_version_mini_installer_path, chromedriver_path,
                 quiet, output_dir):
        """Constructor.

        The constructor initializes a variable dictionary that maps variables to
        their values. These are the only acceptable variables:
        * $BRAND: the browser brand (e.g., 'Google Chrome' or 'Chromium' or
          "Google Chrome for Testing").
        * $CHROME_DIR: the directory of Chrome (or 'Chromium' or
          'Chrome for Testing') from the base installation directory.
        * $CHROME_HTML_PROG_ID: 'ChromeHTML' (or 'ChromiumHTM' or 'CfTHTML').
        * $CHROME_LONG_NAME: 'Google Chrome' (or 'Chromium' or
          'Google Chrome for Testing').
        * $CHROME_LONG_NAME_BETA: 'Google Chrome Beta' if $BRAND is 'Google
        *   Chrome'.
        * $CHROME_LONG_NAME_DEV: 'Google Chrome Dev' if $BRAND is 'Google
        *   Chrome'.
        * $CHROME_LONG_NAME_SXS: 'Google Chrome SxS' if $BRAND is 'Google
        *   Chrome'.
        * $CHROME_SHORT_NAME: 'Chrome' (or 'Chromium' or
          'Google Chrome for Testing').
        * $CHROME_SHORT_NAME_BETA: 'ChromeBeta' if $BRAND is 'Google Chrome'.
        * $CHROME_SHORT_NAME_DEV: 'ChromeDev' if $BRAND is 'Google Chrome'.
        * $CHROME_SHORT_NAME_SXS: 'ChromeCanary' if $BRAND is 'Google Chrome'.
        * $CHROME_UPDATE_REGISTRY_SUBKEY: the registry key, excluding the root
            key, of Chrome for Google Update.
        * $CHROME_UPDATE_REGISTRY_SUBKEY_DEV: the registry key, excluding the
            root key, of Chrome Dev for Google Update.
        * $CHROME_UPDATE_REGISTRY_SUBKEY_BETA: the registry key, excluding the
            root key, of Chrome Beta for Google Update.
        * $CHROME_UPDATE_REGISTRY_SUBKEY_SXS: the registry key, excluding the
            root key, of Chrome SxS for Google Update.
        * $CHROMEDRIVER_PATH: Path to chromedriver.
        * $QUIET: Supress output.
        * $OUTPUT_DIR: "--output-dir=DIR" or an empty string.
        * $LAUNCHER_UPDATE_REGISTRY_SUBKEY: the registry key, excluding the root
            key, of the app launcher for Google Update if $BRAND is 'Google
        *   Chrome'.
        * $LOCAL_APPDATA: the unquoted path to the Local Application Data
            folder.
        * $LOG_FILE: "--log-file=FILE" or an empty string.
        * $MINI_INSTALLER: the unquoted path to the mini_installer.
        * $MINI_INSTALLER_BITNESS: the bitness of the mini_installer.
             32 for x86, 64 for x64 or ARM64
        * $MINI_INSTALLER_FILE_VERSION: the file version of $MINI_INSTALLER.
        * $PREVIOUS_VERSION_MINI_INSTALLER: the unquoted path to a
             mini_installer whose version is lower than $MINI_INSTALLER.
        * $PREVIOUS_VERSION_MINI_INSTALLER_FILE_VERSION: the file version of
            $PREVIOUS_VERSION_MINI_INSTALLER.
        * $PROGRAM_FILES: the unquoted path to the Program Files folder.
        * $PYTHON_INTERPRETER: the python interpreter. This is used to propagate
            vpython VirtualEnv properly.
        * $USER_SPECIFIC_REGISTRY_SUFFIX: the output from the function
            _GetUserSpecificRegistrySuffix().
        * $VERSION_[XP/SERVER_2003/VISTA/WIN7/WIN8/WIN8_1/WIN10]: a 2-tuple
            representing the version of the corresponding OS.
        * $WINDOWS_VERSION: a 2-tuple representing the current Windows version.
        * $CHROME_TOAST_ACTIVATOR_CLSID: NotificationActivator's CLSID for
            Chrome.
        * $CHROME_TOAST_ACTIVATOR_CLSID_BETA: NotificationActivator's CLSID for
            Chrome Beta.
        * $CHROME_TOAST_ACTIVATOR_CLSID_DEV: NotificationActivator's CLSID for
            Chrome Dev.
        * $CHROME_TOAST_ACTIVATOR_CLSID_SXS: NotificationActivator's CLSID for
            Chrome SxS.
        * $CHROME_ELEVATOR_CLSID: Elevator Service CLSID for Chrome.
        * $CHROME_ELEVATOR_CLSID_BETA: Elevator Service CLSID for Chrome Beta.
        * $CHROME_ELEVATOR_CLSID_DEV: Elevator Service CLSID for Chrome Dev.
        * $CHROME_ELEVATOR_CLSID_SXS: Elevator Service CLSID for Chrome SxS.
        * $CHROME_ELEVATOR_IID: IElevator IID for Chrome.
        * $CHROME_ELEVATOR_IID_BETA: IElevator IID for Chrome Beta.
        * $CHROME_ELEVATOR_IID_DEV: IElevator IID for Chrome Dev.
        * $CHROME_ELEVATOR_IID_SXS: IElevator IID for Chrome SxS.
        * $CHROME_ELEVATION_SERVICE_NAME: Elevation Service Name for Chrome.
        * $CHROME_ELEVATION_SERVICE_NAME_BETA: Elevation Service Name for Chrome
            Beta.
        * $CHROME_ELEVATION_SERVICE_NAME_DEV: Elevation Service Name for Chrome
            Dev.
        * $CHROME_ELEVATION_SERVICE_NAME_SXS: Elevation Service Name for Chrome
            SxS.
        * $CHROME_ELEVATION_SERVICE_DISPLAY_NAME: Elevation Service Display Name
            for Chrome.
        * $CHROME_ELEVATION_SERVICE_DISPLAY_NAME_BETA: Elevation Service Display
            Name for Chrome Beta.
        * $CHROME_ELEVATION_SERVICE_DISPLAY_NAME_DEV: Elevation Service Display
            Name for Chrome Dev.
        * $CHROME_ELEVATION_SERVICE_DISPLAY_NAME_SXS: Elevation Service Display
            Name for Chrome SxS.
        * $LAST_INSTALLER_BREAKING_VERSION: The last installer version that had
            breaking changes.

        Args:
            mini_installer_path: The path to a mini_installer.
            previous_version_mini_installer_path: The path to a mini_installer
                whose version is lower than |mini_installer_path|.
        """
        mini_installer_abspath = os.path.abspath(mini_installer_path)
        previous_version_mini_installer_abspath = os.path.abspath(
            previous_version_mini_installer_path)
        windows_major_ver, windows_minor_ver, _, _, _ = win32api.GetVersionEx()
        self._variable_mapping = {
            'CHROMEDRIVER_PATH':
            chromedriver_path,
            'QUIET':
            '-q' if quiet else '',
            'OUTPUT_DIR':
            '"--output-dir=%s"' % output_dir if output_dir else '',
            'LAST_INSTALLER_BREAKING_VERSION':
            '85.0.4169.0',
            'LOCAL_APPDATA':
            shell.SHGetFolderPath(0, shellcon.CSIDL_LOCAL_APPDATA, None, 0),
            'LOG_FILE':
            '',
            'MINI_INSTALLER':
            mini_installer_abspath,
            'MINI_INSTALLER_FILE_VERSION':
            _GetFileVersion(mini_installer_abspath),
            'MINI_INSTALLER_BITNESS':
            _GetFileBitness(mini_installer_abspath),
            'PREVIOUS_VERSION_MINI_INSTALLER':
            previous_version_mini_installer_abspath,
            'PREVIOUS_VERSION_MINI_INSTALLER_FILE_VERSION':
            _GetFileVersion(previous_version_mini_installer_abspath),
            'PROGRAM_FILES':
            shell.SHGetFolderPath(
                0, shellcon.CSIDL_PROGRAM_FILES
                if _GetFileBitness(mini_installer_abspath) == '64' else
                shellcon.CSIDL_PROGRAM_FILESX86, None, 0),
            'PYTHON_INTERPRETER':
            sys.executable,
            'USER_SPECIFIC_REGISTRY_SUFFIX':
            _GetUserSpecificRegistrySuffix(),
            'VERSION_SERVER_2003':
            '(5, 2)',
            'VERSION_VISTA':
            '(6, 0)',
            'VERSION_WIN10':
            '(10, 0)',
            'VERSION_WIN7':
            '(6, 1)',
            'VERSION_WIN8':
            '(6, 2)',
            'VERSION_WIN8_1':
            '(6, 3)',
            'VERSION_XP':
            '(5, 1)',
            'WINDOWS_VERSION':
            '(%s, %s)' % (windows_major_ver, windows_minor_ver)
        }

        mini_installer_product_name = _GetProductName(mini_installer_abspath)
        if mini_installer_product_name == 'Google Chrome Installer':
            self._variable_mapping.update({
                'BRAND':
                'Google Chrome',
                'BINARIES_UPDATE_REGISTRY_SUBKEY':
                ('Software\\Google\\Update\\Clients\\'
                 '{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}'),
                'CHROME_DIR':
                'Google\\Chrome',
                'CHROME_HTML_PROG_ID':
                'ChromeHTML',
                'CHROME_HTML_PROG_ID_BETA':
                'ChromeBHTML',
                'CHROME_HTML_PROG_ID_DEV':
                'ChromeDHTML',
                'CHROME_HTML_PROG_ID_SXS':
                'ChromeSSHTM',
                'CHROME_LONG_NAME':
                'Google Chrome',
                'CHROME_SHORT_NAME':
                'Chrome',
                'CHROME_UPDATE_REGISTRY_SUBKEY':
                ('Software\\Google\\Update\\Clients\\'
                 '{8A69D345-D564-463c-AFF1-A69D9E530F96}'),
                'CHROME_CLIENT_STATE_KEY_BETA':
                ('Software\\Google\\Update\\ClientState\\'
                 '{8237E44A-0054-442C-B6B6-EA0509993955}'),
                'CHROME_CLIENT_STATE_KEY_DEV':
                ('Software\\Google\\Update\\ClientState\\'
                 '{401C381F-E0DE-4B85-8BD8-3F3F14FBDA57}'),
                'CHROME_CLIENT_STATE_KEY_SXS':
                ('Software\\Google\\Update\\ClientState\\'
                 '{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}'),
                'CHROME_CLIENT_STATE_KEY':
                ('Software\\Google\\Update\\ClientState\\'
                 '{8A69D345-D564-463c-AFF1-A69D9E530F96}'),
                'CHROME_TOAST_ACTIVATOR_CLSID':
                ('{A2C6CB58-C076-425C-ACB7-6D19D64428CD}'),
                'CHROME_DIR_BETA':
                'Google\\Chrome Beta',
                'CHROME_DIR_DEV':
                'Google\\Chrome Dev',
                'CHROME_DIR_SXS':
                'Google\\Chrome SxS',
                'CHROME_LONG_NAME_BETA':
                'Google Chrome Beta',
                'CHROME_LONG_NAME_DEV':
                'Google Chrome Dev',
                'CHROME_LONG_NAME_SXS':
                'Google Chrome SxS',
                'CHROME_SHORT_NAME_BETA':
                'ChromeBeta',
                'CHROME_SHORT_NAME_DEV':
                'ChromeDev',
                'CHROME_SHORT_NAME_SXS':
                'ChromeCanary',
                'CHROME_UPDATE_REGISTRY_SUBKEY_BETA':
                ('Software\\Google\\Update\\Clients\\'
                 '{8237E44A-0054-442C-B6B6-EA0509993955}'),
                'CHROME_UPDATE_REGISTRY_SUBKEY_DEV':
                ('Software\\Google\\Update\\Clients\\'
                 '{401C381F-E0DE-4B85-8BD8-3F3F14FBDA57}'),
                'CHROME_UPDATE_REGISTRY_SUBKEY_SXS':
                ('Software\\Google\\Update\\Clients\\'
                 '{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}'),
                'LAUNCHER_UPDATE_REGISTRY_SUBKEY':
                ('Software\\Google\\Update\\Clients\\'
                 '{FDA71E6F-AC4C-4a00-8B70-9958A68906BF}'),
                'CHROME_TOAST_ACTIVATOR_CLSID_BETA':
                ('{B89B137F-96AA-4AE2-98C4-6373EAA1EA4D}'),
                'CHROME_TOAST_ACTIVATOR_CLSID_DEV':
                ('{F01C03EB-D431-4C83-8D7A-902771E732FA}'),
                'CHROME_TOAST_ACTIVATOR_CLSID_SXS':
                ('{FA372A6E-149F-4E95-832D-8F698D40AD7F}'),
                'CHROME_ELEVATOR_CLSID':
                ('{708860E0-F641-4611-8895-7D867DD3675B}'),
                'CHROME_ELEVATOR_CLSID_BETA':
                ('{DD2646BA-3707-4BF8-B9A7-038691A68FC2}'),
                'CHROME_ELEVATOR_CLSID_DEV':
                ('{DA7FDCA5-2CAA-4637-AA17-0740584DE7DA}'),
                'CHROME_ELEVATOR_CLSID_SXS':
                ('{704C2872-2049-435E-A469-0A534313C42B}'),
                'CHROME_ELEVATOR_IID':
                ('{463ABECF-410D-407F-8AF5-0DF35A005CC8}'),
                'CHROME_ELEVATOR_IID_BETA':
                ('{A2721D66-376E-4D2F-9F0F-9070E9A42B5F}'),
                'CHROME_ELEVATOR_IID_DEV':
                ('{BB2AA26B-343A-4072-8B6F-80557B8CE571}'),
                'CHROME_ELEVATOR_IID_SXS':
                ('{4F7CE041-28E9-484F-9DD0-61A8CACEFEE4}'),
                'CHROME_ELEVATION_SERVICE_NAME':
                ('GoogleChromeElevationService'),
                'CHROME_ELEVATION_SERVICE_NAME_BETA':
                ('GoogleChromeBetaElevationService'),
                'CHROME_ELEVATION_SERVICE_NAME_DEV':
                ('GoogleChromeDevElevationService'),
                'CHROME_ELEVATION_SERVICE_NAME_SXS':
                ('GoogleChromeCanaryElevationService'),
                'CHROME_ELEVATION_SERVICE_DISPLAY_NAME':
                ('Google Chrome Elevation Service ' +
                 '(GoogleChromeElevationService)'),
                'CHROME_ELEVATION_SERVICE_DISPLAY_NAME_BETA':
                ('Google Chrome Beta Elevation Service'
                 ' (GoogleChromeBetaElevationService)'),
                'CHROME_ELEVATION_SERVICE_DISPLAY_NAME_DEV':
                ('Google Chrome Dev Elevation Service'
                 ' (GoogleChromeDevElevationService)'),
                'CHROME_ELEVATION_SERVICE_DISPLAY_NAME_SXS':
                ('Google Chrome Canary Elevation Service'),
            })
        elif mini_installer_product_name == 'Chromium Installer':
            self._variable_mapping.update({
                'BRAND':
                'Chromium',
                'BINARIES_UPDATE_REGISTRY_SUBKEY':
                'Software\\Chromium Binaries',
                'CHROME_DIR':
                'Chromium',
                'CHROME_HTML_PROG_ID':
                'ChromiumHTM',
                'CHROME_LONG_NAME':
                'Chromium',
                'CHROME_SHORT_NAME':
                'Chromium',
                'CHROME_UPDATE_REGISTRY_SUBKEY':
                'Software\\Chromium',
                'CHROME_CLIENT_STATE_KEY':
                'Software\\Chromium',
                'CHROME_TOAST_ACTIVATOR_CLSID':
                ('{635EFA6F-08D6-4EC9-BD14-8A0FDE975159}'),
                'CHROME_ELEVATOR_CLSID':
                ('{D133B120-6DB4-4D6B-8BFE-83BF8CA1B1B0}'),
                'CHROME_ELEVATOR_IID':
                ('{B88C45B9-8825-4629-B83E-77CC67D9CEED}'),
                'CHROME_ELEVATION_SERVICE_NAME':
                'ChromiumElevationService',
                'CHROME_ELEVATION_SERVICE_DISPLAY_NAME':
                ('Chromium Elevation Service (ChromiumElevationService)'),
            })
        elif mini_installer_product_name == ('Google Chrome for Testing '
                                             'Installer'):
            self._variable_mapping.update({
                'BRAND':
                'Google Chrome for Testing',
                'CHROME_DIR':
                'Google\\Chrome for Testing',
                'CHROME_HTML_PROG_ID':
                'CfTHTML',
                'CHROME_LONG_NAME':
                'Google Chrome for Testing',
                'CHROME_SHORT_NAME':
                'Google Chrome for Testing',
                'CHROME_UPDATE_REGISTRY_SUBKEY':
                'Software\\Chrome for Testing',
                'CHROME_CLIENT_STATE_KEY':
                'Software\\Chrome for Testing',
                'CHROME_TOAST_ACTIVATOR_CLSID':
                ('{77ED8F9B-E27A-499F-8E2F-D7C04157CF64}'),
                'CHROME_ELEVATOR_CLSID':
                ('{724349BF-E1CF-4481-A64D-8CD10183CA03}'),
                'CHROME_ELEVATOR_IID':
                ('{3DC48E97-47D0-476F-8F89-0792FC611567}'),
                'CHROME_ELEVATION_SERVICE_NAME':
                'GoogleChromeforTestingElevationService',
                'CHROME_ELEVATION_SERVICE_DISPLAY_NAME':
                ('Google Chrome for Testing Elevation Service ' +
                 '(GoogleChromeforTestingElevationService)'),
            })
        else:
            raise KeyError("Unknown mini_installer product name '%s'" %
                           mini_installer_product_name)

    def SetLogFile(self, log_file):
        """Updates the value for the LOG_FILE variable"""
        self._variable_mapping['LOG_FILE'] = ('"--log-file=%s"' %
                                              log_file if log_file else '')

    def Expand(self, a_string):
        """Expands variables in the given string.

        This method resolves only variables defined in the constructor. It does
        not resolve environment variables. Any dollar signs that are not part of
        variables must be escaped with $$, otherwise a KeyError or a ValueError
        will be raised.

        Args:
            a_string: A string.

        Returns:
            A new string created by replacing variables with their values.
        """
        return string.Template(a_string).substitute(self._variable_mapping)
