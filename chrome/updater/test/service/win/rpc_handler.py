# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import sys

import impersonate
import proc_util
import uac


class UpdaterTestRPCHandler():
    def echo(self, message):
        """Test method to check if server is reachable."""
        return message

    def RunAsSystem(self, command, env=None, cwd=None, timeout=30):
        """Runs the command as SYSTEM user.

      Args:
          command: The command to run. This argument will be forwarded to
            subprocess.Popen().
          env: Environment variables to pass to command.
          cwd: Working directory for the command.
          timeout: How long the child process should wait before timeout.

      Returns:
          (pid, exit_code, stdout, stderr) tuple.
      """
        try:
            process = subprocess.Popen(command,
                                       stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE,
                                       env=env,
                                       cwd=cwd)

            stdout, stderr = process.communicate(timeout)
            logging.info('Command %s stdout:\n %s', command, stdout)
            if stderr:
                logging.error('Command %s stderr:\n %s', command, stderr)

            return (process.pid, process.returncode, stdout, stderr)
        except (OSError, subprocess.TimeoutExpired) as err:
            logging.exception(err)
            return (None, None, None, None)

    def RunAsStandardUser(self, command_line, env=None, cwd=None, timeout=30):
        """Runs the command as the non-elevated logon user on default desktop.

      Args:
          command_line: The command line string, includes all arguments.
          env: Environment variables to pass to command.
          cwd: Working directory for the command.
          timeout: How long the child process should wait before timeout.

      Returns:
          (pid, exit_code, stdout, stderr) tuple.
      """
        return impersonate.RunAsStandardUser(command_line, env, cwd, timeout)

    def AnswerUpcomingUACPrompt(self,
                                actions,
                                timeout=10,
                                wait_child=False,
                                source=''):
        """Answers upcoming UAC prompt that does not require username/password.

        Args:
            actions: Actions to take in string, such as 'AADDA', 'A' to accept,
                     'D' to deny.
            timeout: How long the child process should wait for each UAC click.
            wait_child: Whether this thread should wait the completion of child
                        proc.
            source: Optional name of the source that triggers this action
                    (for logging and debugging purposes).

        Returns:
            (pid, exit_code) of the created UAC-answering process.
            If the sub-process is not created, or did not finish in wait time,
            returns (None, None).
        """
        uac_tool = os.path.join(os.path.dirname(__file__), 'answer_uac.py')
        command = ('python %s --actions=%s --timeout=%d --source=%s' %
                   (uac_tool, actions, timeout, source))
        logging.info('Running command: %s', command)

        if wait_child:
            if timeout > 0:
                # Each button click could take `timeout` seconds, and add
                # 1 second extra for child process to finish.
                timeout = timeout * len(actions) + 1
            else:
                # Negative timeout has special meanings, such as
                # win32event.INFINITE.
                # Don't touch it.
                pass
        else:
            timeout = 0  # no wait

        # There could be multiple winlogon.exe instances when there are multiple
        # login sessions. For example, when there's remote desktop session.
        # In this case, find the active session where the UAC prompt is supposed
        # to display.
        winlogon_pids = proc_util.GetPIDsWithName(
            'winlogon.exe', proc_util.GetActiveSessionID())
        if not winlogon_pids:
            logging.error(
                'Unexpected: no active session or no winlogon.exe in it.')
            return (None, None)
        elif len(winlogon_pids) > 1:
            logging.warning(
                'Unexpected multiple winlogon.exe instances within '
                'active session, the first instance will be used.')

        # Must spawn child process on the same desktop as the one that UAC
        # prompts, otherwise the child process will not be able to find the UAC
        # dialog. Please note that there is a slight race condition here as user
        # could change UAC desktop at any time. But we can tolerate this for the
        # testing purpose.
        desktop = 'winlogon' if uac.IsPromptingOnSecureDesktop() else 'default'

        logging.info('Spawn process [%s] for UAC on desktop [%s].', command,
                     desktop)
        pid, exit_code, stdout, stderr = impersonate.RunAsPidOnDeskstop(
            command, winlogon_pids[0], desktop=desktop, timeout=timeout)
        logging.info('Process [%s] is created to answer UAC, exit_code: %s',
                     pid, exit_code)
        if stdout and stdout.strip():
            logging.info('STDOUT: [%s]', stdout)
        if stderr and stderr.strip():
            logging.error('STDERR: [%s]', stderr)
        return (pid, exit_code)
