#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates a zip file that can be used by manual testers.

python chrome/test/mini_installer/create_zip.py

This will drop <OUTPUT_DIR>\mini_installer_tests.zip onto the disk.
To generate an easy-to-distribute package for manual testers add the current
and previous installers as mini_installer.exe and
previous_version_mini_installer.exe, as well as chromedriver.exe.

This can be passed out and extracted into c:\mini_installer_tests and go through
the README.

chromedriver.exe can be obtained one of two ways. Either download it from
http://chromedriver.chromium.org/downloads. It can also be built locally, but
if you do this make sure you are not building as components else you will
have to copy some dlls to the chromedriver.exe dir.

Note: This does not zip the executables by default. However paths to the
current, previous, and chromedriver binaries can be passed to be zipped.

The easiest way to package everything is to run:
  python chrome\test\mini_installer\create_zip.py ^
    -o <ZIP_FILE_OUTPUT_PATH> ^
    -i <CURRENT_INSTALLER_PATH> ^
    -p <PREVIOUS_INSTALLER_PATH> ^
    -c <CHROMEDRIVER_PATH>

This will drop a zip file making the distribution of the test needs simple.
When the runner batch script is run it will install the python packages
required by the tests to further reduce the overhead of running the tests.
The directory structure is also preserved, so running the tests from
run_tests.bat all of the import paths are correct. __init__.py files
are dropped in any empty folders to make them importable.
"""

import argparse
import logging
import os
import re
import sys
import zipfile

THIS_DIR = os.path.abspath(os.path.dirname(os.path.abspath(__file__)))
SRC_DIR = os.path.join(THIS_DIR, '..', '..', '..')
SELENIUM_PATH = os.path.abspath(
    os.path.join(SRC_DIR, 'third_party', 'webdriver', 'pylib'))
TYP_PATH = os.path.abspath(
    os.path.join(SRC_DIR, 'third_party', 'catapult', 'third_party', 'typ'))
BLOCKLIST = ['', '.pyc', '.gn', '.gni', '.txt', '.bat']


def ArchiveDirectory(path, zipf):
    """Archive an entire directory and subdirectories.

    This will skip files that have an extension in BLOCKLIST.

    Args:
        path: The path to the current directory.
        zipf: A handle to a ZipFile instance.
    """
    logging.debug('Archiving %s', path)
    for c_path in [os.path.join(path, name) for name in os.listdir(path)]:
        if os.path.isfile(c_path):
            if os.path.splitext(c_path)[-1] in BLOCKLIST:
                continue
            logging.debug('Adding %s', os.path.relpath(c_path, SRC_DIR))
            zipf.write(c_path, os.path.relpath(c_path, SRC_DIR))
        elif os.path.isdir(c_path):
            ArchiveDirectory(c_path, zipf)


def main():
    logging.basicConfig(
        format='[%(asctime)s:%(filename)s(%(lineno)d)] %(message)s',
        datefmt='%m%d/%H%M%S',
        level=logging.INFO)

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--output-path',
                        default='installer_tests.zip',
                        help='The path to write the zip file to')
    parser.add_argument('--installer-path',
                        default='',
                        help='The path to the current installer. This is '
                        'optional. If passed it will be zipped as '
                        'mini_installer.exe')
    parser.add_argument('--previous-version-installer-path',
                        default='',
                        help='The path to the previous installer. This is '
                        'optional. If passed it will be zipped as '
                        'previous_version_mini_installer.exe')
    parser.add_argument('--chromedriver-path',
                        default='',
                        help='The path to chromedriver.exe. This is '
                        'optional.')
    args = parser.parse_args()

    with zipfile.ZipFile(args.output_path, 'w') as zipf:

        # Setup chrome\test\mini_installer as importable in Python
        zipf.writestr(os.path.join('chrome', '__init__.py'), '')
        zipf.writestr(os.path.join('chrome', 'test', '__init__.py'), '')
        zipf.writestr(
            os.path.join('chrome', 'test', 'mini_installer', '__init__.py'),
            '')

        run_args = []
        # Add any of the executables
        if args.installer_path:
            installer_name = os.path.split(args.installer_path)[-1]
            run_args.append('--installer-path=' + installer_name)
            logging.debug('Archiving: %s', installer_name)
            zipf.write(args.installer_path, installer_name)

        if args.previous_version_installer_path:
            previous_version_installer_name = os.path.split(
                args.previous_version_installer_path)[-1]
            run_args.append('--previous-version-installer-path=' +
                            previous_version_installer_name)
            logging.debug('Archiving: %s', previous_version_installer_name)
            zipf.write(args.previous_version_installer_path,
                       previous_version_installer_name)

        if args.chromedriver_path:
            chromedriver_name = os.path.split(args.chromedriver_path)[-1]
            run_args.append('--chromedriver-path=' + chromedriver_name)
            logging.debug('Archiving: %s', chromedriver_name)
            zipf.write(args.chromedriver_path, chromedriver_name)

        # Add the top level files
        with open(os.path.join(THIS_DIR, 'zip_test_runner.bat')) as rh:
            text = rh.read().format(run_args=' '.join(run_args))
            text = re.sub("\r(?!\n)|(?<!\r)\n", "\r\n", text)
            zipf.writestr('zip_test_runner.bat', text)
        zipf.write(os.path.join(THIS_DIR, 'ZIP_README.txt'),
                   os.path.split('README.txt')[-1])

        # Archive this, chromedriver, and typ.
        logging.debug('Zipping chrome/test/mini_installer')
        ArchiveDirectory(THIS_DIR, zipf)
        logging.debug('Zipping third_party/catapult/third_party/typ')
        ArchiveDirectory(TYP_PATH, zipf)
        logging.debug('Zipping third_party/webdriver/pylib')
        ArchiveDirectory(SELENIUM_PATH, zipf)
    logging.debug('Wrote zip to %s', args.output_path)

    return 0


if __name__ == '__main__':
    sys.exit(main())
