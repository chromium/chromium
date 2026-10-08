# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test doubles shared by the media perf unit tests."""

import errno
import os
import socket
import subprocess
import sys
import types
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import remote_transport  # pylint: disable=wrong-import-position

MAC_HOST = 'lin-137-e504--mac'
WIN_HOST = 'lin-149-e504--win'
USER = 'chrome-bot'
IP = '10.0.0.37'
NO_ROUTE = 'No route to host'
SKIP_LOG = 'WARNING:root:Skipping remote cleanup: sender unreachable.'
PUBKEY_DENIED = f'{USER}@{MAC_HOST}: Permission denied (publickey).'


def make_args(sender=MAC_HOST, username=USER, sender_os='mac'):
    return types.SimpleNamespace(
        sender=sender, username=username, sender_os=sender_os
    )


def completed(returncode=0, stdout='', stderr=''):
    return subprocess.CompletedProcess(
        args=['ssh'], returncode=returncode, stdout=stdout, stderr=stderr
    )


def no_route():
    return OSError(errno.EHOSTUNREACH, NO_ROUTE)


class FakeSsh:
    """Stands in for subprocess.run and answers ssh commands by remote cmd.

    A response may be a list, which is returned in order; its last entry
    repeats. scp calls are recorded in `copies` and succeed.
    """

    def __init__(self):
        self.responses = {'echo ok': completed(stdout='ok\n')}
        self.default = completed()
        self.remote_commands = []
        self.copies = []

    def __call__(self, argv, **kwargs):
        del kwargs
        if argv[0] == 'scp':
            self.copies.append(argv)
            return completed()
        assert argv[0] == 'ssh', argv
        command = argv[-1]
        self.remote_commands.append(command)
        response = self.responses.get(command, self.default)
        if isinstance(response, list):
            return response.pop(0) if len(response) > 1 else response[0]
        return response

    def ran(self, command):
        return command in self.remote_commands

    def ran_prefix(self, prefix):
        return any(c.startswith(prefix) for c in self.remote_commands)


class SenderTestCase(unittest.TestCase):
    """Patches sockets, subprocess, and sleep, and resets the preflight cache.

    By default the sender resolves, accepts TCP connections, and answers
    `echo ok`, so the connectivity check passes.
    """

    def setUp(self):
        # pylint: disable=protected-access
        remote_transport._preflight_results.clear()
        self.addCleanup(remote_transport._preflight_results.clear)
        # pylint: enable=protected-access

        self.getaddrinfo = self._patch(
            'socket.getaddrinfo',
            return_value=[
                (socket.AF_INET, socket.SOCK_STREAM, 6, '', (IP, 22))
            ],
        )
        self.conn = mock.MagicMock()
        self.create_connection = self._patch(
            'socket.create_connection', return_value=self.conn
        )
        self.ssh = FakeSsh()
        self.run = self._patch('subprocess.run', side_effect=self.ssh)
        self.popen = self._patch('subprocess.Popen')
        # Spawned processes keep running, so open_tunnel sees the tunnel up.
        self.popen.return_value.wait.side_effect = subprocess.TimeoutExpired(
            'ssh', remote_transport.TUNNEL_STARTUP_CHECK_SECS
        )
        self.sleep = self._patch('time.sleep')

    def _patch(self, target, **kwargs):
        patcher = mock.patch(target, **kwargs)
        self.addCleanup(patcher.stop)
        return patcher.start()

    def make_unreachable(self):
        self.create_connection.side_effect = no_route()

    def fail_preflight(self, args=None):
        """Runs a failing connectivity check so the failure is cached."""
        args = args or make_args()
        self.make_unreachable()
        with self.assertLogs(level='WARNING'):
            with self.assertRaises(remote_transport.SenderUnreachableError):
                remote_transport.make_transport(
                    args.sender, args.username
                ).verify_connectivity()
        self.run.reset_mock()
        self.ssh.remote_commands.clear()
