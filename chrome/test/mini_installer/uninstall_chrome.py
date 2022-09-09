#!/usr/bin/env python3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Uninstalls Chrome.

This script reads the uninstall command from registry, calls it, and verifies
the output status code.
"""

import logging
import optparse
import subprocess
import sys
import winreg


def main():
    parser = optparse.OptionParser(description='Uninstall Chrome.')
    parser.add_option('--system-level',
                      action='store_true',
                      dest='system_level',
                      default=False,
                      help='Uninstall Chrome at system level.')
    parser.add_option('--chrome-long-name',
                      default='Google Chrome',
                      help='Google Chrome or Chromium)')
    parser.add_option('--interactive',
                      action='store_true',
                      dest='interactive',
                      default=False,
                      help='Ask before uninstalling Chrome.')
    parser.add_option(
        '--no-error-if-absent',
        action='store_true',
        dest='no_error_if_absent',
        default=False,
        help='No error if the registry key for uninstalling Chrome '
        'is absent.')
    parser.add_option(
        '--log-file',
        dest='log_file',
        help='File into which the installer is to write its logs',
        metavar='FILE')
    options, _ = parser.parse_args()

    # TODO(sukolsak): Add support for uninstalling MSI-based Chrome installs
    # when we support testing MSIs.
    if options.system_level:
        root_key = winreg.HKEY_LOCAL_MACHINE
    else:
        root_key = winreg.HKEY_CURRENT_USER
    sub_key = ('SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%s' %
               options.chrome_long_name)
    # Query the key. It will throw a WindowsError if the key doesn't exist.
    try:
        key = winreg.OpenKey(root_key, sub_key, 0,
                             winreg.KEY_QUERY_VALUE | winreg.KEY_WOW64_32KEY)
    except WindowsError as e:
        if options.no_error_if_absent:
            return 0
        raise KeyError('Registry key %s\\%s is missing' %
                       ('HKEY_LOCAL_MACHINE' if options.system_level else
                        'HKEY_CURRENT_USER', sub_key)) from e
    if options.interactive:
        prompt = (
            'Warning: This will uninstall %s at %s. Do you want to continue? '
            '(y/N) ' %
            (options.chrome_long_name,
             'system-level' if options.system_level else 'user-level'))
        if input(prompt).strip() != 'y':
            logging.error('User aborted')
            return 1
    uninstall_string, _ = winreg.QueryValueEx(key, 'UninstallString')
    uninstall_string += ' --force-uninstall'
    if options.log_file:
        uninstall_string += (' --verbose-logging --log-file="%s"' %
                             options.log_file)
    exit_status = subprocess.call(uninstall_string, shell=True)
    # The exit status for successful uninstallation of Chrome is 19 (see
    # chrome/installer/util/util_constants.h).
    if exit_status != 19:
        raise Exception(
            'Could not uninstall Chrome. The installer exited with '
            'status %d.' % exit_status)
    return 0


if __name__ == '__main__':
    sys.exit(main())
