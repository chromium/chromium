#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test Chrome using chromedriver.

If the webdriver API or the chromedriver.exe binary can't be found this
becomes a no-op. This is to allow running locally while the waterfalls get
setup. Once all locations have been provisioned and are expected to contain
these items the checks should be removed to ensure this is run on each test
and fails if anything is incorrect.
"""

import argparse
import contextlib
import logging
import os
import shutil
import sys
import tempfile
import time

from selenium import webdriver
from selenium.webdriver import ChromeOptions
import chrome_helper

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
TEST_HTML_FILE = 'file://' + os.path.join(THIS_DIR, 'test_page.html')


@contextlib.contextmanager
def CreateChromedriver(args):
    """Create a webdriver object and close it after."""

    def DeleteWithRetry(path, func):
        # There seems to be a race condition on the bots that causes the paths
        # to not delete because they are being used. This allows up to 4 seconds
        # to delete
        for _ in range(8):
            try:
                return func(path)
            except WindowsError:
                time.sleep(0.5)
        raise

    def CollectCrashReports(user_data_dir, output_dir):
        """Searches for Chrome crash reports, collecting them for analysis.

        Args:
            user_data_dir: The full path of the User Data dir.
            output_dir: If not None, a path to which collected crash reports are
                to be moved.

        Returns:
            The number of crash reports found.
        """
        report_dir = os.path.join(user_data_dir, 'Crashpad', 'reports')
        dumps = []
        try:
            dumps = os.listdir(report_dir)
        except OSError:
            # Assume this is file not found, meaning no crash reports.
            return 0
        for dump in dumps:
            dump_path = os.path.join(report_dir, dump)
            if (output_dir):
                target_path = os.path.join(output_dir, dump)
                try:
                    shutil.copyfile(dump_path, target_path)
                    logging.error('Saved Chrome crash dump to %s', target_path)
                except OSError:
                    logging.exception(
                        'Failed to copy Chrome crash dump from %s to %s',
                        dump_path, target_path)
            else:
                logging.error('Found Chrome crash dump at %s', dump_path)
        return len(dumps)

    driver = None
    user_data_dir = tempfile.mkdtemp()
    fd, log_file = tempfile.mkstemp()
    os.close(fd)
    chrome_options = ChromeOptions()
    chrome_options.binary_location = args.chrome_path
    chrome_options.add_argument('user-data-dir=' + user_data_dir)
    chrome_options.add_argument('log-file=' + log_file)
    chrome_options.add_argument('enable-logging')
    chrome_options.add_argument('v=1')
    emit_log = False
    try:
        driver = webdriver.Chrome(args.chromedriver_path,
                                  chrome_options=chrome_options)
        yield driver
    except:
        emit_log = True
        raise
    finally:
        if driver:
            driver.quit()
        chrome_helper.WaitForChromeExit(args.chrome_path)
        report_count = CollectCrashReports(user_data_dir, args.output_dir)
        if report_count:
            emit_log = True
        try:
            DeleteWithRetry(user_data_dir, shutil.rmtree)
        except:
            emit_log = True
            raise
        finally:
            if emit_log:
                with open(log_file) as fh:
                    logging.error(fh.read())
                if args.output_dir:
                    target = os.path.join(args.output_dir,
                                          os.path.basename(log_file))
                    shutil.copyfile(log_file, target)
                    logging.error('Saved Chrome log to %s', target)
            try:
                DeleteWithRetry(log_file, os.remove)
            except WindowsError:
                # Don't fail the test if the log file couldn't be deleted.
                logging.exception('Failed to delete log file %s' % log_file)
        if report_count:
            raise Exception('Failing test due to %s crash reports found' %
                            report_count)


def main():
    """Main entry point."""
    parser = parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-q',
                        '--quiet',
                        action='store_true',
                        default=False,
                        help='Reduce test runner output')
    parser.add_argument('--chromedriver-path',
                        default='chromedriver.exe',
                        metavar='FILENAME',
                        help='Path to chromedriver')
    parser.add_argument(
        '--output-dir',
        metavar='DIR',
        help='Directory into which crash dumps and other output '
        ' files are to be written')
    parser.add_argument('chrome_path',
                        metavar='FILENAME',
                        help='Path to chrome installer')
    args = parser.parse_args()

    # This test is run from src, but this script is called with a cwd of
    # chrome/test/mini_installer, so relative paths need to be compensated for.
    if not os.path.exists(args.chromedriver_path):
        args.chromedriver_path = os.path.join('..', '..', '..',
                                              args.chromedriver_path)
    if not os.path.exists(args.chrome_path):
        args.chrome_path = os.path.join('..', '..', '..', args.chrome_path)

    logging.basicConfig(
        format='[%(asctime)s:%(filename)s(%(lineno)d)] %(message)s',
        datefmt='%m%d/%H%M%S',
        level=logging.ERROR if args.quiet else logging.INFO)

    if not args.chrome_path:
        logging.error('The path to the chrome binary is required.')
        return -1
    # Check that chromedriver is correct.
    if not os.path.exists(args.chromedriver_path):
        # If we can't find chromedriver exit as a no-op.
        logging.info('Cant find %s. Exiting test_chrome_with_chromedriver',
                     args.chromedriver_path)
        return 0
    with CreateChromedriver(args) as driver:
        driver.get(TEST_HTML_FILE)
        assert driver.title == 'Chromedriver Test Page', (
            'The page title was not correct.')
        element = driver.find_element_by_tag_name('body')
        assert element.text == 'This is the test page', (
            'The page body was not correct')
    return 0


if __name__ == '__main__':
    sys.exit(main())
