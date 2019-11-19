# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The commands module wraps operations that have side-effects.
"""

import os
import plistlib
import shutil
import subprocess
import tempfile

from . import logger


def file_exists(path):
    return os.path.exists(path)


def copy_files(source, dest):
    assert source[-1] != '/'
    subprocess.check_call(
        ['rsync', '--archive', '--checksum', '--delete', source, dest])


def copy_dir_overwrite_and_count_changes(source, dest, dry_run=False):
    assert source[-1] != '/'
    command = [
        'rsync', '--archive', '--checksum', '--itemize-changes', '--delete',
        source + '/', dest
    ]
    if dry_run:
        command.append('--dry-run')
    output = subprocess.check_output(command)

    # --itemize-changes will print a '.' in the first column if the item is not
    # being updated, created, or deleted. This happens if only attributes
    # change, such as a timestamp or permissions. Timestamp changes are
    # uninteresting for the purposes of determining changed content, but
    # permissions changes are not. Columns 6-8 are also checked so that files
    # that have potentially interesting attributes (permissions, owner, or
    # group) changing are counted, but column 5 for the timestamp is not
    # considered.
    changes = 0
    for line in output.split('\n'):
        if line == '' or (line[0] == '.' and line[5:8] == '...'):
            continue
        changes += 1
    return changes


def move_file(source, dest):
    shutil.move(source, dest)


def make_dir(at):
    os.mkdir(at)


def write_file(path, contents):
    with open(path, 'w') as f:
        f.write(contents)


def run_command(args, **kwargs):
    logger.info('Running command: %s', args)
    subprocess.check_call(args, **kwargs)


def run_command_output(args, **kwargs):
    logger.info('Running command: %s', args)
    return subprocess.check_output(args, **kwargs)


class PlistContext(object):
    """
    PlistContext is a context manager that reads a plist on entry, providing
    the contents as a dictionary. If |rewrite| is True, then the same dictionary
    is re-serialized on exit. If |create_new| is True, then the file is not read
    but rather an empty dictionary is created.
    """

    def __init__(self, plist_path, rewrite=False, create_new=False):
        self._path = plist_path
        self._rewrite = rewrite
        self._create_new = create_new

    def __enter__(self):
        if self._create_new:
            self._plist = {}
        else:
            self._plist = plistlib.readPlist(self._path)
        return self._plist

    def __exit__(self, exc_type, exc_value, exc_tb):
        if self._rewrite and not exc_type:
            plistlib.writePlist(self._plist, self._path)
        self._plist = None


class WorkDirectory(object):
    """
    WorkDirectory creates a temporary directory on entry, storing the path as
    the |model.Paths.work| path. On exit, the directory is destroyed.
    """

    def __init__(self, paths):
        self._workdir = tempfile.mkdtemp(prefix='chromesign_')
        self._paths = paths

    def __enter__(self):
        return self._paths.replace_work(self._workdir)

    def __exit__(self, exc_type, value, traceback):
        shutil.rmtree(self._workdir)
