# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""argparse.ArgumentParser for the mini_installer test suite.

The provided parser is based on that created by typ.
"""

import typ


def ArgumentParser(host=None):
    """Returns an argparse.ArgumentParser for the mini_installer test suite.

    Args:
        host: A typ.Host to pass to typ's argument parser.

    Returns:
        A filled out ArgumentParser instance.
    """
    parser = typ.ArgumentParser(host)
    group = parser.add_argument_group(title='run_mini_installer_tests')
    group.add_argument('--force-clean',
                       action='store_true',
                       default=False,
                       help='Force cleaning existing installations')
    group.add_argument(
        '--output-dir',
        metavar='DIR',
        help='Directory into which crash dumps and other output '
        ' files are to be written')
    group.add_argument('--installer-path',
                       default='mini_installer.exe',
                       metavar='FILENAME',
                       help='The path of the installer.')
    group.add_argument('--previous-version-installer-path',
                       default='previous_version_mini_installer.exe',
                       metavar='FILENAME',
                       help='The path of the previous version installer.')
    group.add_argument('--chromedriver-path',
                       default='chromedriver.exe',
                       help='The path to chromedriver.')
    group.add_argument('--config',
                       default='config.config',
                       metavar='FILENAME',
                       help='Path to test configuration file')
    return parser
