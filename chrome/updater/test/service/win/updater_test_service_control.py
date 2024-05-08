# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# [VPYTHON:BEGIN]
# python_version: "3.8"
# wheel: <
#   name: "infra/python/wheels/pywin32/${vpython_platform}"
#    version: "version:300"
# >
# [VPYTHON:END]

import contextlib
import logging
import os
import shutil
import subprocess
import socket
import sys
import time

import pywintypes
import win32api
import win32service
import win32serviceutil

import rpc_client

_UPDATER_TEST_SERVICE_NAME = 'UpdaterTestService'

# Errors that might be raised when interacting with the service.
# pylint: disable=undefined-variable
_ServiceErrors = (OSError, pywintypes.error, win32api.error,
                  win32service.error, WindowsError)
# pylint: enable=undefined-variable


def _RunCommand(command, log_error=True):
    """Run a command and logs stdout/stderr if needed.

    Args:
        command: Command to run.
        log_error: Whether to log the stderr.

    Returns:
        True if the process exits with 0.
    """
    process = subprocess.Popen(command,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    logging.info('Command %s stdout:\n %s', command, stdout)
    if log_error and stderr:
        logging.error('Command %s stderr:\n %s', command, stderr)

    return process.returncode == 0


def _SetupEnvironmentForVPython():
    """Setup vpython environment."""
    # vpython_spec above brings the pywin32 module we need, but it may not be
    # ready to use, run the post install scripts as described by
    # https://pypi.org/project/pywin32/.
    # This script outputs some error messages to stderr if it has run before.
    # So skip logging to avoid this log pollution.
    post_install_script = os.path.join(
        os.path.dirname(os.path.abspath(sys.executable)),
        'pywin32_postinstall.py')
    _RunCommand(
        [sys.executable, post_install_script, '-install', '-silent', '-quiet'],
        log_error=False)

    # Make pythonservice.exe explicit for our service. This is to avoid pickup
    # an incompatible interpreter accidentally.
    python_dir = os.path.dirname(os.path.abspath(sys.executable))
    source = os.path.join(os.path.dirname(python_dir), 'Lib', 'site-packages',
                          'win32', 'pythonservice.exe')
    python_service_path = os.path.join(python_dir, 'pythonservice.exe')
    if os.path.exists(source) and not os.path.exists(python_service_path):
        shutil.copyfile(source, python_service_path)
    os.environ['PYTHON_SERVICE_EXE'] = python_service_path


def _IsServiceInStatus(status):
    """Returns the if test service is in the given status."""
    try:
        return status == win32serviceutil.QueryServiceStatus(
            _UPDATER_TEST_SERVICE_NAME)[1]
    except _ServiceErrors as err:
        return False


def _MainServiceScriptPath():
    """Returns the service main script path."""
    # Assumes updater_test_service.py file is in the same directory as
    # this file.
    service_main = os.path.join(os.path.abspath(os.path.dirname(__file__)),
                                'updater_test_service.py')
    if not os.path.isfile(service_main):
        logging.error('Cannot find service main module: %s', service_main)
        return None
    return service_main


def _WaitServiceStatus(status, timeout=30):
    """Wait the service to be in the given state."""
    check_interval = 0.2
    for i in range(int(timeout / check_interval)):
        if _IsServiceInStatus(status):
            return True
        time.sleep(check_interval)
    return False


def InstallService():
    """Install updater test service.

    If the service was previously installed, it will be updated.

    Returns:
        True if the service is installed successfully.
    """
    _SetupEnvironmentForVPython()

    service_main = _MainServiceScriptPath()
    if not service_main:
        logging.error('Cannot find the service main script [%s].',
                      service_main)
        return False

    try:
        if _IsServiceInStatus(
                win32service.SERVICE_RUNNING) and not StopService():
            logging.error('Cannot stop existing test service.')
            return False

        logging.info('Installing service with script: %s', service_main)
        command = [
            sys.executable, service_main, '--interactive', '--startup', 'auto',
            'install'
        ]
        if _RunCommand(command):
            logging.info('Service [%s] installed.', _UPDATER_TEST_SERVICE_NAME)
            return True
        else:
            logging.error('Failed to install [%s].',
                          _UPDATER_TEST_SERVICE_NAME)
            return False
    except _ServiceErrors as err:
        logging.exception(err)
        return False


def UninstallService():
    """Uninstall the service."""
    service_main = _MainServiceScriptPath()
    if not service_main:
        logging.error('Unexpected: missing service main script [%s].',
                      service_main)
        return False

    try:
        if _IsServiceInStatus(
                win32service.SERVICE_RUNNING) and not StopService():
            logging.error('Cannot stop test service for uninstall.')
            return False

        command = [sys.executable, service_main, 'remove']
        if _RunCommand(command):
            logging.error('Service [%s] uninstalled.',
                          _UPDATER_TEST_SERVICE_NAME)
            return True
        else:
            logging.error('Failed to uninstall [%s].',
                          _UPDATER_TEST_SERVICE_NAME)
            return False
    except _ServiceErrors as err:
        logging.error('Failed to install service.')
        logging.exception(err)
        return False


def StartService(timeout=30):
    """Start updater test service and make sure it is reachable.

    Args:
        timeout: How long to wait for service to be ready.

    Returns:
        True if the service is started successfully.
    """
    logging.info('Starting service [%s].', _UPDATER_TEST_SERVICE_NAME)
    if _IsServiceInStatus(win32service.SERVICE_RUNNING):
        logging.info('Test service is already running.')
        return True

    try:
        win32serviceutil.StartService(_UPDATER_TEST_SERVICE_NAME)
        if not _WaitServiceStatus(win32service.SERVICE_RUNNING, timeout):
            logging.error('Wait for service start failed.')
            return False

        logging.error('Service %s started.', _UPDATER_TEST_SERVICE_NAME)
        return rpc_client.TestConnection()
    except _ServiceErrors as err:
        logging.error('Failed to start service.')
        logging.exception(err)

    return False


def StopService(timeout=30):
    """Stop test service if it is running.

    Returns:
        True if the service is stopped successfully.
    """
    logging.info('Stopping service [%s]...', _UPDATER_TEST_SERVICE_NAME)
    try:
        if not _IsServiceInStatus(win32service.SERVICE_RUNNING):
            return True

        win32serviceutil.StopService(_UPDATER_TEST_SERVICE_NAME)
        if not _WaitServiceStatus(win32service.SERVICE_STOPPED, timeout):
            logging.error('Wait for service stop failed.')
            return False

        logging.info('Service [%s] stopped.', _UPDATER_TEST_SERVICE_NAME)
        return True
    except _ServiceErrors as err:
        logging.error('Failed to stop service.')
        logging.exception(err)
        return False


@contextlib.contextmanager
def OpenService():
    """Open the service as a managed resource."""
    try:
        if InstallService() and StartService():
            yield _UPDATER_TEST_SERVICE_NAME
        else:
            yield None
    finally:
        UninstallService()


if __name__ == '__main__':
    if len(sys.argv) == 1:
        logging.error('Must provide an action.')
        sys.exit(-1)

    command = sys.argv[1]
    if command == 'setup':
        result = InstallService() and StartService()
    elif command == 'teardown':
        result = UninstallService()
    else:
        logging.error('Unknown command: %s.', command)
        sys.exit(-2)

    sys.exit(0 if result else 1)
