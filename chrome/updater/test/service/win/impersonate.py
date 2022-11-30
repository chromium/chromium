# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ctypes
import logging
import os
import threading

import msvcrt
import pywintypes
import win32api
import win32con
import win32event
import win32file
import win32pipe
import win32process
import win32profile
import win32security
import win32ts
import winerror

import proc_util


class ImpersonationError(Exception):
    """Error representing impersonation error."""

    pass


class _StdoutStderrPipes(object):
    """Helper class to read stdout/stderr from child process."""

    def __init__(self):
        # Pipes to stream outputs.
        security_attributes = win32security.SECURITY_ATTRIBUTES()
        security_attributes.bInheritHandle = 1
        stdout_r, stdout_w = win32pipe.CreatePipe(security_attributes, 0)
        stderr_r, stderr_w = win32pipe.CreatePipe(security_attributes, 0)
        self.stdout_r = stdout_r.Detach()
        self.stdout_w = self._MakeInheritable(stdout_w)
        self.stderr_r = stderr_r.Detach()
        self.stderr_w = self._MakeInheritable(stderr_w)

        # Threads to read pipes and the actual contents returned.
        self.stdout_read_thread = None
        self.stdout = None
        self.stderr_read_thread = None
        self.stderr = None

    def _MakeInheritable(self, handle):
        """Returns an inheritable duplicated handle."""
        inheritable_handle = win32api.DuplicateHandle(
            win32api.GetCurrentProcess(), handle, win32api.GetCurrentProcess(),
            0, 1, win32con.DUPLICATE_SAME_ACCESS)
        win32file.CloseHandle(handle)
        return inheritable_handle

    def _ReadStdout(self):
        """Read content from the stdout pipe."""
        try:
            logging.info('Into read thread for STDOUT...')
            stdout_buf = os.fdopen(msvcrt.open_osfhandle(self.stdout_r, 0),
                                   'rt')
            self.stdout = stdout_buf.read()
        except Exception as err:
            logging.exception(err)

    def _ReadStderr(self):
        """Read content from the stderr pipe."""
        try:
            logging.info('Into read thread for STDERR...')
            stderr_buf = os.fdopen(msvcrt.open_osfhandle(self.stderr_r, 0),
                                   'rt')
            self.stderr = stderr_buf.read()
        except Exception as err:
            logging.exception(err)

    def ReadAll(self):
        """Fork threads to read stdout/stderr."""
        self.stdout_thread = threading.Thread(target=self._ReadStdout)
        self.stdout_thread.setDaemon(True)
        self.stdout_thread.start()

        self.stderr_thread = threading.Thread(target=self._ReadStderr)
        self.stderr_thread.setDaemon(True)
        self.stderr_thread.start()

    def CloseWriteHandles(self):
        """Closes write handles.

        This is important to unblock readers: read threads will wait until all
        write handles are closed.
        """
        win32file.CloseHandle(self.stdout_w)
        win32file.CloseHandle(self.stderr_w)


def _RunAsOnWindowStationDesktop(command_line,
                                 security_token,
                                 window_station,
                                 desktop,
                                 env=None,
                                 cwd=None,
                                 timeout=win32event.INFINITE):
    """Runs a command as the security token user on given desktop.

    Args:
        command_line: Full command line string to run.
        security_token: Security token that the command run as.
        window_station: Window station for the new process to run, tpically is
                        "WinSta0", aka the interactive window station.
        desktop: Desktop that the new process will be associatedw with,
                 typically is 'default'.
        env: The environment variables to pass to the child process, or None to
             inherit from parent.
        cwd: The working directory of the child process, or None to inherit from
             parent.
        timeout: How long should wait for child process. 0 means no wait,
                 None means infinitely.

    Returns:
        (pid, exit_code, stdout, stderr) tuple.

    Raises:
        ImpersonationError: when impersonation failed.
    """
    pipes = _StdoutStderrPipes()
    si = win32process.STARTUPINFO()
    si.dwFlags = (win32process.STARTF_USESHOWWINDOW
                  | win32con.STARTF_USESTDHANDLES)
    si.wShowWindow = win32con.SW_SHOW
    si.lpDesktop = '%s\\%s' % (window_station, desktop)
    si.hStdOutput = pipes.stdout_w
    si.hStdError = pipes.stderr_w
    create_flags = (win32process.CREATE_NEW_CONSOLE
                    | win32process.CREATE_UNICODE_ENVIRONMENT)

    if env:
        saved_env = dict(os.environ)
        os.environ.update(env)
    env_block = win32profile.CreateEnvironmentBlock(security_token, True)
    (process_handle, unused_thread, pid,
     unused_thread_id) = win32process.CreateProcessAsUser(
         security_token, None, command_line, None, None, 1, create_flags,
         env_block, cwd, si)
    if env:
        os.environ.clear()
        os.environ.update(saved_env)
    pipes.CloseWriteHandles()
    if not process_handle:
        logging.error('Failed to create child process [%s] on [%s\\%s]',
                      command_line, window_station, desktop)
        raise ImpersonationError(
            'Failed to create process [%s] with impersonation: [%s\\%s][%s]' %
            (command_line, window_station, desktop, cwd))

    pipes.ReadAll()
    logging.info('Child process [%s] created on [%s\\%s]', command_line,
                 window_station, desktop)
    logging.info('Waiting %s seconds for child process.', timeout)
    if timeout != win32event.INFINITE:
        timeout *= 1000  # Convert from seconds to milli-seconds.
    wait_result = win32event.WaitForSingleObject(process_handle,
                                                 timeout * 1000)
    if wait_result == win32event.WAIT_OBJECT_0:
        exit_code = win32process.GetExitCodeProcess(process_handle)
        logging.info('Child process exited with code %s.', exit_code)
        logging.info('Child process STDOUT: %s', pipes.stdout)
        logging.error('Child process STDERR: %s.', pipes.stderr)
        return (pid, exit_code, pipes.stdout, pipes.stderr)
    else:
        if timeout != 0:
            logging.warning('Wait for child process timeout in %s seconds',
                            timeout / 1000)
        return (pid, None, None, None)


def RunAsStandardUser(command_line, env, cwd, timeout):
    """Runs a command as non-elevated logged-on user.

    Args:
        command_line: The command line string, including arguments, to run.
        env: Environment variables for child process, None to inherit.
        cwd: Working directory for child process, None to inherit from parent.
        timeout: How long in seconds should wait for child process.

    Returns:
        (pid, exit_code, sdtout, stderr) tuple.

    Raises:
        ImpersonationError: when impersonation failed.
    """
    logging.info('Running "%s" as the logon user.', command_line)

    # Adjust current process to be part of the trusted computer base.
    current_process_token = win32security.OpenProcessToken(
        win32api.GetCurrentProcess(), win32security.TOKEN_ALL_ACCESS)
    tcb_privilege_flag = win32security.LookupPrivilegeValue(
        None, win32security.SE_TCB_NAME)
    se_enable = win32security.SE_PRIVILEGE_ENABLED
    win32security.AdjustTokenPrivileges(current_process_token, 0,
                                        [(tcb_privilege_flag, se_enable)])

    active_session_id = proc_util.GetActiveSessionID()
    if not active_session_id:
        raise ImpersonationError('Cannot find active logon session.')

    try:
        logon_user_token = win32ts.WTSQueryUserToken(active_session_id)
    except pywintypes.error as err:
        if err.winerror == winerror.ERROR_NO_TOKEN:
            raise ImpersonationError('No user is logged on.')
        else:
            raise ImpersonationError('Failed to get user token: %s' % err)

    return _RunAsOnWindowStationDesktop(command_line, logon_user_token,
                                        'WinSta0', 'default', env, cwd,
                                        timeout)


def RunAsPidOnDeskstop(command_line,
                       pid,
                       window_station='WinSta0',
                       desktop='default',
                       env=None,
                       cwd=None,
                       timeout=win32event.INFINITE):
    """Runs a command with pid's security token and on the given desktop.

    Args:
        command_line: The command line string, including arguments, to run.
        pid: ID of the process to get the security token from.
        window_station: Window station for the new process to run, tpically is
           "WinSta0", aka the interactive window station.
        desktop: Desktop that the new process will be associatedw with,
                 typically is 'default'. If the command needs to interact with
                 UAC prompt on secure desktop, pass 'winlogon'.
        env: The environment variables to pass to the child process, or None to
             inherit from parent.
        cwd: The working directory of the child process, or None to inherit from
             parent.
        timeout: How long in seconds to wait for child process.

    Returns:
        (pid, exit_code, stdout, stderr) tuple.
    """

    logging.info('RunAsPidOnDeskstop: [%s][%s]', pid, command_line)

    process_handle = None
    token_handle = None
    try:
        process_handle = win32api.OpenProcess(win32con.GENERIC_ALL, False, pid)
        token_handle = win32security.OpenProcessToken(
            process_handle, win32con.TOKEN_ALL_ACCESS)
        return _RunAsOnWindowStationDesktop(command_line, token_handle,
                                            window_station, desktop, env, cwd,
                                            timeout)
    except (pywintypes.error, ImpersonationError) as err:
        logging.error(err)
        return (None, None, None, None)
    finally:
        if process_handle:
            process_handle.Close()
        if token_handle:
            token_handle.Close()
