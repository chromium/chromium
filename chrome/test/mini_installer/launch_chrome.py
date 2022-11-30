#!/usr/bin/env python3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Launches Chrome.

This script launches Chrome and waits until its window shows up.
"""

import optparse
import sys
import time
import win32process

import chrome_helper


def WaitForWindow(process_id, class_pattern):
    """Waits until a window specified by |process_id| and class name shows up.

    Args:
        process_id: The ID of the process that owns the window.
        class_pattern: The regular expression pattern of the window class name.

    Returns:
        A boolean value indicating whether the specified window shows up within
        30 seconds.
    """
    start_time = time.time()
    while time.time() - start_time < 30:
        if chrome_helper.WindowExists([process_id], class_pattern):
            return True
        time.sleep(0.1)
    return False


def main():
    usage = 'usage: %prog chrome_path'
    parser = optparse.OptionParser(usage, description='Launch Chrome.')
    _, args = parser.parse_args()
    if len(args) != 1:
        parser.error('Incorrect number of arguments.')
    chrome_path = args[0]

    # Use CreateProcess rather than subprocess.Popen to avoid side effects such
    # as handle inheritance.
    _, _, process_id, _ = win32process.CreateProcess(
        None, chrome_path, None, None, 0, 0, None, None,
        win32process.STARTUPINFO())
    if not WaitForWindow(process_id, 'Chrome_WidgetWin_'):
        raise Exception('Could not launch Chrome.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
