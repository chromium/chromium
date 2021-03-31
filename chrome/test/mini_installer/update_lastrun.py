# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Updates Chrome's "lastrun" value for the current user in HKCU."""

import _winreg
import optparse
import sys
import time


def UpdateLastrun(client_state_key_path):
    """ Updates Chrome's "lastrun" value in the registry to the current time.

    Args:
        client_state_key_path: The path to Chrome's ClientState key in the
            registry.
    """
    # time.time returns seconds since the Unix epoch. Chrome uses microseconds
    # since the Windows epoch. Adjust based on values inspired by
    # https://support.microsoft.com/en-us/kb/167296, which uses 100-nanosecond
    # ticks.
    now_us = str(int(time.time() * 1000000 + 11644473600000000L))
    try:
        with _winreg.OpenKey(_winreg.HKEY_CURRENT_USER, client_state_key_path,
                             0, _winreg.KEY_SET_VALUE
                             | _winreg.KEY_WOW64_32KEY) as key:
            _winreg.SetValueEx(key, 'lastrun', 0, _winreg.REG_SZ, now_us)
    except WindowsError:
        raise KeyError('Failed opening registry key HKEY_CURRENT_USER\\%s' %
                       client_state_key_path)
    return 0


def main():
    usage = 'usage: %prog client_state_key_path'
    parser = optparse.OptionParser(
        usage, description='Update Chrome\'s "lastrun" value.')
    _, args = parser.parse_args()
    if len(args) != 1:
        parser.error('Incorrect number of arguments.')

    UpdateLastrun(args[0])
    return 0


if __name__ == '__main__':
    sys.exit(main())
