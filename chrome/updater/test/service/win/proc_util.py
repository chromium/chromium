# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import csv
import logging
import subprocess

import win32ts


def GetActiveSessionID():
    """Get the active session ID."""
    local_server = win32ts.WTS_CURRENT_SERVER_HANDLE
    for session in win32ts.WTSEnumerateSessions(local_server):
        if session['State'] == win32ts.WTSActive:
            return session['SessionId']

    logging.warning('Unexpected: no active session.')
    return None


def GetPIDsWithName(image_name, session=None):
    """Gets all process PIDs, with the given image name.

    Args:
        image_name: Case-insensitive process image name.
        session: Session filter. Only search processes within given session.
            None means no filter.

    Returns:
        A list of process ID.
    """
    cmd = ['tasklist', '/FO:csv', '/NH', '/FI', 'IMAGENAME eq %s' % image_name]
    if session is not None:
        cmd.extend(['/FI', 'SESSION eq %s' % session])
    proc = subprocess.Popen(cmd,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)

    stdout, stderr = proc.communicate()
    if proc.returncode != 0:
        logging.error('Unable to list processes, %s', stderr)
        return []

    stdout = stdout.decode('ascii').splitlines()
    return [int(row[1]) for row in csv.reader(stdout, delimiter=',')]
