# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The commands module wraps operations that have side-effects.
"""

import os
import platform
import plistlib
import shutil
import stat
import subprocess
import tempfile

from signing import logger


def file_exists(path):
    return os.path.exists(path)


def delete_file_if_exists(path):
    try:
        os.unlink(path)
    except FileNotFoundError:
        pass


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
    output = subprocess.check_output(command).decode('utf-8')

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


def read_file(path):
    with open(path, 'r') as f:
        return f.read()


def zip(out, path):
    run_command(['zip', '-9ry', out, '.'], cwd=path)


def set_executable(path):
    """Makes the file at the specified path executable.

    Args:
        path: The path to the file to make executable.
    """
    os.chmod(path, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR
             | stat.S_IRGRP | stat.S_IXGRP | stat.S_IROTH
             | stat.S_IXOTH)  # -rwxr-xr-x a.k.a. 0755


def run_command(args, **kwargs):
    logger.info('Running command: %s', args)
    subprocess.check_call(args, **kwargs)


def run_command_output(args, **kwargs):
    logger.info('Running command: %s', args)
    return subprocess.check_output(args, **kwargs)


def lenient_run_command_output(args, **kwargs):
    """Runs a command, being fairly tolerant of errors.

    Returns:
        A tuple of (returncode, stdoutdata, stderrdata), or if an OSError was
        raised, (None, None, None).
    """
    logger.info('Running command: %s', args)

    try:
        process = subprocess.Popen(
            args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, **kwargs)
    except OSError:
        return (None, None, None)

    (stdout, stderr) = process.communicate()

    return (process.wait(), stdout, stderr)


def macos_version():
    """Determines the macOS version of the running system.

    Returns:
        A list containing one element for each component of the version number,
        such as [10, 15, 6] and [11, 0].
    """
    return [int(x) for x in platform.mac_ver()[0].split('.')]


def read_plist(path):
    """Loads Plist at |path| and returns it as a dictionary."""
    with open(path, 'rb') as f:
        return plistlib.load(f)


def write_plist(data, path, format):
    """Saves |data| as a Plist to |path| in the specified |format|."""
    # The below does not replace the destination file but update it in place,
    # so if more than one hardlink points to destination all of them will be
    # modified. This is not what is expected, so delete destination file if
    # it does exist.
    delete_file_if_exists(path)
    with open(path, 'wb') as f:
        plist_format = {
            'binary1': plistlib.FMT_BINARY,
            'xml1': plistlib.FMT_XML
        }
        plistlib.dump(data, f, fmt=plist_format[format])


class PlistContext(object):
    """
    PlistContext is a context manager that reads a plist on entry, providing
    the contents as a dictionary. If |rewrite| is True, then the same dictionary
    is re-serialized on exit. If |create_new| is True, then the file is not read
    but rather an empty dictionary is created. If |binary| is True, then both
    input and output will be in binary instead of the default XML format.
    """

    def __init__(self,
                 plist_path,
                 rewrite=False,
                 create_new=False,
                 binary=False):
        self._path = plist_path
        self._rewrite = rewrite
        self._create_new = create_new
        self._format = 'binary1' if binary else 'xml1'

    def __enter__(self):
        if self._create_new:
            self._plist = {}
        else:
            self._plist = read_plist(self._path)
        return self._plist

    def __exit__(self, exc_type, exc_value, exc_tb):
        if self._rewrite and not exc_type:
            write_plist(self._plist, self._path, self._format)
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
