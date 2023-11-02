# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import psutil

import chrome_helper


LOGGER = logging.getLogger('installer_test')


def VerifyProcessExpectation(expectation_name, expectation, variable_expander):
    """Verifies that a file is present or absent, throwing an AssertionError if
    the expectation is not met.

    Args:
        expectation_name: Path to the process being verified (may contain
            variables to be expanded).
        expectation: A dictionary with the following key and value:
            'running' a boolean indicating whether the process should be
                running.
        variable_expander: A VariableExpander object.

    Raises:
        AssertionError: If an expectation is not satisfied.
    """
    running_process_paths = [
        path for (_, path) in chrome_helper.GetProcessIDAndPathPairs()
    ]
    process_path = variable_expander.Expand(expectation_name)
    is_running = process_path in running_process_paths
    assert expectation['running'] == is_running, \
        ('Process %s is running' % process_path) if is_running else \
        ('Process %s is not running' % process_path)


def CleanProcess(expectation_name, expectation, variable_expander):
    """Terminates processes based on expectations.

    Args:
        expectation_name: Path to a process's executable.
        expectation: A dictionary describing the state of the process:
            'running': A boolean False indicating that the process must not be
                running.
        variable_expander: A VariableExpander object.

    Raises:
        AssertionError: If an expectation is not satisfied.
        WindowsError: If an error occurs while deleting the path.
    """
    process_path = variable_expander.Expand(expectation_name)
    assert not expectation['running'], (
        'Invalid expectation for CleanProcess operation: \'running\' property '
        + 'for %s must not be True' % process_path)

    for proc in psutil.process_iter():
        try:
            if not proc.exe() == process_path:
                continue
        except psutil.Error:
            # Ignore processes for which the path cannot be determined.
            # AccessDenied is expected for pid 0, pid 4, and any others that the
            # current process does not have access to query.
            continue
        pid = proc.pid
        try:
            proc.kill()
            LOGGER.info('CleanProcess killed process %s of pid %s' %
                        (process_path, pid))
        except psutil.NoSuchProcess:
            LOGGER.info('CleanProcess tried to kill process %s of pid %s, ' %
                        (process_path, pid) + 'yet it was already gone')
