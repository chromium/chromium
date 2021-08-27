# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import subprocess

import impersonate


class UpdaterTestRPCHandler():

  def echo(self, message):
    """Test method to check if server is reachable."""
    return message

  def RunAsSystem(self, command, env=None, cwd=None, timeout=30):
    """Runs the command as SYSTEM user.

    Args:
      command: The command to run.
      env: Environment variables to pass to command.
      cwd: Working directory for the command.
      timeout: How long the child process should wait for UAC before timeout.

    Returns:
      (pid, exit_code, sdtout, stderr) tuple.
    """
    try:
      process = subprocess.Popen(
          command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
          env=env, cwd=cwd)

      # TODO(crbug.com/1233612): `communicate()` in Python 2.7 does not support
      # timeout value, pass the value here once we migrate to Python 3. Also
      # don't forget to handle subprocess.TimeoutExpired exception.
      stdout, stderr = process.communicate()
      logging.info('Command %s stdout:\n %s', command, stdout)
      if stderr:
        logging.error('Command %s stderr:\n %s', command, stderr)

      return (process.pid, process.returncode, stdout, stderr)
    except OSError as err:
      logging.exception(err)
      return (None, None, None, None)

  def RunAsConsoleUser(self, command, env=None, cwd=None, timeout=200):
    """Runs the command as the console user on default desktop.

    Args:
      command: The command to run.
      env: Environment variables to pass to command.
      cwd: Working directory for the command.
      timeout: How long the child process should wait for UAC before timeout.

    Returns:
      (pid, exit_code, sdtout, stderr) tuple.
    """
    return impersonate.RunAsConsoleUser(command, env, cwd, timeout)