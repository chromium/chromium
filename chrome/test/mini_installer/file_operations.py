# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import shutil
import stat
import winerror


LOGGER = logging.getLogger('installer_test')


def VerifyFileExpectation(expectation_name, expectation, variable_expander):
    """Verifies that a file is present or absent, throwing an AssertionError if
    the expectation is not met.

    Args:
        expectation_name: Path to the file being verified (may contain variables
            to be expanded).
        expectation: A dictionary with the following key and value:
            'exists' a boolean indicating whether the file should exist.
        variable_expander: A VariableExpander object.

    Raises:
        AssertionError: If an expectation is not satisfied.
    """

    def GetDirContents(path):
        """Returns a list of all files and directories in a directory."""
        contents = []
        for dirpath, dirnames, filenames in os.walk(path):
            for a_dir in dirnames:
                contents.append('%s\\' % os.path.join(dirpath, a_dir))
            for a_file in filenames:
                contents.append(os.path.join(dirpath, a_file))
        contents.sort()
        return '\n'.join(contents)

    file_path = variable_expander.Expand(expectation_name)
    file_exists = os.path.exists(file_path)
    if file_exists:
        is_dir = False
        try:
            is_dir = stat.S_ISDIR(os.lstat(file_path).st_mode)
        except WindowsError:
            pass
        if is_dir:
            assert expectation['exists'], (
                'Directory %s exists with contents: %s\n' %
                (file_path, GetDirContents(file_path)))
        else:
            assert expectation['exists'], ('File %s exists' % file_path)
    else:
        assert not expectation['exists'], ('File %s is missing' % file_path)


def CleanFile(expectation_name, expectation, variable_expander):
    """Deletes files or directories based on expectations.

    Args:
        expectation_name: Path to the file/directory to be cleaned.
        expectation: A dictionary describing the state of the path:
            'exists': A boolean False indicating that the path must not exist.
        variable_expander: A VariableExpander object.

    Raises:
        AssertionError: If an expectation is not satisfied.
        WindowsError: If an error occurs while deleting the path.
    """
    file_path = variable_expander.Expand(expectation_name)
    assert not expectation['exists'], (
        'Invalid expectation for CleanFile operation: \'exists\' property for '
        + 'path %s must not be True' % file_path)
    try:
        if stat.S_ISDIR(os.lstat(file_path).st_mode):
            shutil.rmtree(file_path)
            LOGGER.info('CleanFile deleted directory %s' % file_path)
        else:
            os.remove(file_path)
            LOGGER.info('CleanFile deleted file %s' % file_path)
    except WindowsError as e:
        if (e.winerror != winerror.ERROR_FILE_NOT_FOUND
                and e.winerror != winerror.ERROR_PATH_NOT_FOUND):
            raise
