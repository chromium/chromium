#!/usr/bin/env python3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Quits Chrome.

This script sends a WM_CLOSE message to each window of Chrome and waits until
the process terminates.
"""

import optparse
import os
import pywintypes
import sys
import time
import win32con
import win32gui
import winerror

import chrome_helper


def CloseWindows(process_path):
    """Closes all windows owned by processes whose exe path is |process_path|.

    Args:
        process_path: The path to the executable whose processes will have their
            windows closed.

    Returns:
        A boolean indicating whether the processes successfully terminated
        within 25 seconds.
    """
    start_time = time.time()
    while time.time() - start_time < 25:
        process_ids = chrome_helper.GetProcessIDs(process_path)
        if not process_ids:
            return True

        for hwnd in chrome_helper.GetWindowHandles(process_ids):
            try:
                win32gui.PostMessage(hwnd, win32con.WM_CLOSE, 0, 0)
            except pywintypes.error as error:
                # It's normal that some window handles have become invalid.
                if error.args[0] != winerror.ERROR_INVALID_WINDOW_HANDLE:
                    raise
        time.sleep(0.1)
    return False


def KillNamedProcess(process_path):
    """ Kills all running exes with the same name as the exe at |process_path|.

    Args:
        process_path: The path to an executable.

    Returns:
        True if running executables were successfully killed. False otherwise.
    """
    return os.system('taskkill /f /im %s' %
                     os.path.basename(process_path)) == 0


def QuitChrome(chrome_path):
    """ Tries to quit chrome in a safe way. If there is still an open instance
        after a timeout delay, the process is killed the hard way.

    Args:
        chrome_path: The path to chrome.exe.
    """
    if not CloseWindows(chrome_path):
        # TODO(robertshield): Investigate why Chrome occasionally doesn't shut
        # down.
        sys.stderr.write('Warning: Chrome not responding to window closure. '
                         'Killing all processes belonging to %s\n' %
                         chrome_path)
        KillNamedProcess(chrome_path)


def main():
    usage = 'usage: %prog chrome_path'
    parser = optparse.OptionParser(usage, description='Quit Chrome.')
    _, args = parser.parse_args()
    if len(args) != 1:
        parser.error('Incorrect number of arguments.')
    chrome_path = args[0]

    QuitChrome(chrome_path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
