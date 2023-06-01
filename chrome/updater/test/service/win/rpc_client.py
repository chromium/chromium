# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import socket
import sys
import xmlrpc.client

_UPDATER_XML_RPC_PORT = 9090

# Errors that might be raised when interacting with the RPC server.
RPCErrors = (socket.error, socket.herror, socket.gaierror, socket.timeout)


def GetProxy():
    """Constructs a XML RPC server proxy."""
    return xmlrpc.client.ServerProxy('http://localhost:%s' %
                                     _UPDATER_XML_RPC_PORT)


def TestConnection():
    """Test that connection to the XML RPC server."""
    try:
        proxy = GetProxy()
        return 'hi' == proxy.echo('hi')
    except RPCErrors as err:
        logging.error('Unable connect to XML RPC server')
        logging.exception(err)
        return False


def RunAsSystem(command, env=None, cwd=None, timeout=90):
    """Runs the command as SYSTEM user.

    Args:
        command: The command to run. This argument will be forwarded to
          subprocess.Popen().
        env: Environment variables to pass to command.
        cwd: Working directory for the command.
        timeout: How long the child process should wait for UAC before timeout.

    Returns:
        (pid, exit_code, sdtout, stderr) tuple.
    """
    try:
        proxy = GetProxy()
        env = env or dict(os.environ)
        cwd = cwd or os.getcwd()
        return proxy.RunAsSystem(command, env, cwd, timeout)
    except RPCErrors as err:
        logging.exception(err)
        raise


def RunAsStandardUser(command_line, env=None, cwd=None, timeout=90):
    """Runs the command as the non-elevated logged-on user on default desktop.

    Args:
        command_line: The command line string, includes all arguments.
        env: Environment variables to pass to command.
        cwd: Working directory for the command.
        timeout: How long the child process should wait before timeout.

    Returns:
        (pid, exit_code, sdtout, stderr) tuple.
    """
    try:
        proxy = GetProxy()
        env = env or dict(os.environ)
        cwd = cwd or os.getcwd()
        return proxy.RunAsStandardUser(command_line, env, cwd, timeout)
    except RPCErrors as err:
        logging.exception(err)
        raise


def AnswerUpcomingUACPrompt(actions, timeout=10, wait_child=False, source=''):
    """Answers upcoming UAC prompt that does not require username/password.

    Args:
        actions: Actions to take in string, such as 'AADDA', 'A' to accept,
            'D' to deny.
        timeout: How long the child process should wait for each UAC click.
        wait_child: Whether this thread should wait the completion of
                    the child process.
        source: Optional name of the source that triggers this action
                (for logging and debugging purposes).

    Returns:
        (pid, exit_code) of the created UAC-answering process.
        If the sub-process is not created, or did not finish in wait time,
        returns (None, None).
    """
    try:
        proxy = GetProxy()
        return proxy.AnswerUpcomingUACPrompt(actions, timeout, wait_child,
                                             source)
    except RPCErrors as err:
        logging.exception(err)
        raise
