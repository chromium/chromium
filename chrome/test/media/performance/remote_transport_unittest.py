#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for remote_transport.py."""

import errno
import socket
import subprocess
import unittest
from unittest import mock

import perf_test_fakes as fakes
from perf_test_fakes import IP, MAC_HOST, NO_ROUTE, USER, WIN_HOST, completed
import remote_transport
from remote_transport import (
    INFRA_FAILURE_NOTE,
    RemoteDeviceError,
    SenderNotFoundError,
    SenderSshError,
    SenderUnreachableError,
)


def _ssh(host=MAC_HOST, username=USER):
    return remote_transport.make_transport(host, username)


class VerifyConnectivityTest(fakes.SenderTestCase):
    # Spec case 1.
    def test_dns_failure_raises_not_found_without_ssh(self):
        self.getaddrinfo.side_effect = socket.gaierror(
            socket.EAI_NONAME, 'Name or service not known'
        )

        with self.assertLogs(level='WARNING'):
            with self.assertRaises(SenderNotFoundError) as ctx:
                _ssh().verify_connectivity()

        msg = str(ctx.exception)
        self.assertIn(f"Sender '{MAC_HOST}' does not resolve", msg)
        self.assertIn('Name or service not known', msg)
        self.assertIn('--sender', msg)
        self.assertIn(INFRA_FAILURE_NOTE, msg)
        self.create_connection.assert_not_called()
        self.run.assert_not_called()

    # Spec case 2.
    def test_no_route_raises_unreachable_after_three_attempts(self):
        self.make_unreachable()

        with self.assertLogs(level='WARNING') as logs:
            with self.assertRaises(SenderUnreachableError) as ctx:
                _ssh().verify_connectivity()

        self.assertEqual(self.create_connection.call_count, 3)
        self.create_connection.assert_called_with((MAC_HOST, 22), timeout=5)
        self.assertEqual(self.sleep.call_args_list, [mock.call(5)] * 2)
        self.run.assert_not_called()

        reason = f'[Errno {errno.EHOSTUNREACH}] {NO_ROUTE}'
        self.assertEqual(
            logs.output,
            [
                f'WARNING:root:Sender check attempt {i}/3 failed for '
                f'{MAC_HOST}: {reason}'
                for i in (1, 2, 3)
            ],
        )
        self.assertEqual(
            str(ctx.exception),
            f"Sender '{MAC_HOST}' ({IP}) is not reachable on port 22: "
            f"{reason}. Likely causes: device powered off/asleep/rebooting, "
            "network link or DHCP lease lost, or sshd not running. "
            "This is an infra failure, not a test failure.",
        )

    def test_tcp_timeout_raises_unreachable(self):
        self.create_connection.side_effect = socket.timeout('timed out')
        with self.assertLogs(level='WARNING'):
            with self.assertRaises(SenderUnreachableError) as ctx:
                _ssh().verify_connectivity()
        self.assertIn('timed out', str(ctx.exception))

    # Spec case 3.
    def test_ssh_255_raises_ssh_error_with_stderr(self):
        self.ssh.responses['echo ok'] = completed(
            255, stderr=fakes.PUBKEY_DENIED
        )

        with self.assertLogs(level='WARNING') as logs:
            with self.assertRaises(SenderSshError) as ctx:
                _ssh().verify_connectivity()

        msg = str(ctx.exception)
        self.assertIn('rc=255', msg)
        self.assertIn(fakes.PUBKEY_DENIED, msg)
        self.assertIn(INFRA_FAILURE_NOTE, msg)
        # run() itself also reports the 255 at ERROR.
        self.assertIn(
            f'ERROR:root:SSH to {MAC_HOST} failed (rc=255): '
            f'{fakes.PUBKEY_DENIED}',
            logs.output,
        )

    def test_ssh_unexpected_stdout_raises_ssh_error(self):
        self.ssh.responses['echo ok'] = completed(stdout='Welcome!\n')
        with self.assertLogs(level='WARNING'):
            with self.assertRaises(SenderSshError) as ctx:
                _ssh().verify_connectivity()
        self.assertIn('Welcome!', str(ctx.exception))

    def test_ssh_timeout_raises_ssh_error(self):
        self.run.side_effect = subprocess.TimeoutExpired(cmd='ssh', timeout=120)
        with self.assertLogs(level='WARNING'):
            with self.assertRaises(SenderSshError) as ctx:
                _ssh().verify_connectivity()
        self.assertIn('timed out', str(ctx.exception))

    # Spec case 4.
    def test_healthy_sender_passes_and_is_cached(self):
        with self.assertLogs(level='INFO') as logs:
            _ssh().verify_connectivity()
        # A new transport for the same host/user shares the cached result.
        _ssh().verify_connectivity()

        self.assertEqual(
            logs.output,
            [f'INFO:root:Sender {MAC_HOST} ({IP}) reachable over SSH.'],
        )
        self.getaddrinfo.assert_called_once_with(MAC_HOST, 22)
        self.create_connection.assert_called_once()
        self.conn.close.assert_called_once()
        self.assertEqual(self.ssh.remote_commands, ['echo ok'])
        self.sleep.assert_not_called()

    def test_accepts_windows_line_endings(self):
        self.ssh.responses['echo ok'] = completed(stdout='ok\r\n')
        _ssh().verify_connectivity()

    # Spec case 5.
    def test_local_senders_run_no_checks(self):
        for sender in ('localhost', '127.0.0.1', None):
            with self.subTest(sender=sender):
                transport = _ssh(host=sender)
                self.assertTrue(transport.is_local)
                transport.verify_connectivity()
        self.getaddrinfo.assert_not_called()
        self.create_connection.assert_not_called()
        self.run.assert_not_called()

    def test_recovers_when_sender_returns_mid_retry(self):
        self.create_connection.side_effect = [fakes.no_route(), self.conn]
        with self.assertLogs(level='INFO') as logs:
            _ssh().verify_connectivity()
        self.sleep.assert_called_once_with(5)
        self.assertTrue(logs.output[0].startswith('WARNING'))
        self.assertTrue(logs.output[-1].startswith('INFO'))

    def test_failure_is_cached_and_reraised_without_reprobing(self):
        self.fail_preflight()
        self.create_connection.reset_mock()
        self.sleep.reset_mock()

        transport = _ssh()
        self.assertTrue(transport.known_unreachable)
        with self.assertRaises(SenderUnreachableError):
            transport.verify_connectivity()
        self.create_connection.assert_not_called()
        self.sleep.assert_not_called()

    def test_cache_is_keyed_on_host_and_user(self):
        _ssh().verify_connectivity()
        _ssh(username='other').verify_connectivity()
        _ssh(host=WIN_HOST).verify_connectivity()
        self.assertEqual(self.ssh.remote_commands, ['echo ok'] * 3)

    def test_unexpected_exceptions_are_not_swallowed(self):
        self.run.side_effect = ValueError('bug')
        with self.assertRaises(ValueError):
            _ssh().verify_connectivity()
        self.sleep.assert_not_called()

    def test_exception_hierarchy(self):
        for cls in (
            SenderNotFoundError,
            SenderUnreachableError,
            SenderSshError,
        ):
            self.assertTrue(issubclass(cls, RemoteDeviceError))
            self.assertIn('infra failure, not a test failure', cls.__doc__)
        self.assertTrue(issubclass(RemoteDeviceError, RuntimeError))
        self.assertIn(
            'infra failure, not a test failure', RemoteDeviceError.__doc__
        )


class SshTransportTest(fakes.SenderTestCase):
    def test_run_uses_batch_mode_and_connect_timeout(self):
        _ssh().run('true')
        argv = self.run.call_args[0][0]
        opts = list(zip(argv, argv[1:]))
        self.assertIn(('-o', 'StrictHostKeyChecking=no'), opts)
        self.assertIn(('-o', 'BatchMode=yes'), opts)
        self.assertIn(('-o', 'ConnectTimeout=10'), opts)
        self.assertEqual(argv[-2:], [f'{USER}@{MAC_HOST}', 'true'])

    def test_spawn_uses_same_options(self):
        _ssh().spawn('true')
        argv = self.popen.call_args[0][0]
        self.assertIn('BatchMode=yes', argv)
        self.assertIn('ConnectTimeout=10', argv)

    def test_copy_from_uses_same_options(self):
        _ssh().copy_from('/tmp/a.log', '/out/a.log', timeout=60)
        self.assertEqual(len(self.ssh.copies), 1)
        argv = self.ssh.copies[0]
        self.assertIn('BatchMode=yes', argv)
        self.assertEqual(
            argv[-2:], [f'{USER}@{MAC_HOST}:/tmp/a.log', '/out/a.log']
        )
        self.assertEqual(self.run.call_args.kwargs['timeout'], 60)

    def test_logs_error_on_255_and_still_returns_result(self):
        self.ssh.default = completed(255, stderr=NO_ROUTE)
        with self.assertLogs(level='ERROR') as logs:
            result = _ssh().run('true')
        self.assertEqual(result.returncode, 255)
        self.assertEqual(
            logs.output,
            [f'ERROR:root:SSH to {MAC_HOST} failed (rc=255): {NO_ROUTE}'],
        )

    def test_does_not_log_error_for_remote_command_failures(self):
        self.ssh.default = completed(1)
        with self.assertNoLogs(level='ERROR'):
            _ssh().run('false')

    def test_raise_if_disconnected_only_on_255(self):
        transport = _ssh()
        transport.raise_if_disconnected(completed(1), 'grepping')
        self.assertFalse(transport.known_unreachable)

        with self.assertRaises(SenderUnreachableError) as ctx:
            transport.raise_if_disconnected(
                completed(255, stderr=NO_ROUTE), 'grepping'
            )
        self.assertIn('while grepping', str(ctx.exception))
        self.assertIn(NO_ROUTE, str(ctx.exception))
        self.assertTrue(transport.known_unreachable)

    def test_open_tunnel_does_not_spawn_when_unreachable(self):
        self.make_unreachable()
        with self.assertLogs(level='WARNING'):
            with self.assertRaises(SenderUnreachableError):
                _ssh().open_tunnel(1, 2)
        self.popen.assert_not_called()

    def test_open_tunnel_forwards_both_ports(self):
        with self.assertLogs(level='INFO'):
            tunnel = _ssh().open_tunnel(49573, 8000)
        self.assertIs(tunnel, self.popen.return_value)
        argv = self.popen.call_args[0][0]
        self.assertIn('49573:127.0.0.1:49573', argv)
        self.assertIn('8000:127.0.0.1:8000', argv)
        self.assertIn('BatchMode=yes', argv)
        self.assertIn('ConnectTimeout=10', argv)
        self.assertIn('ExitOnForwardFailure=yes', argv)
        self.assertEqual(argv[-2:], [f'{USER}@{MAC_HOST}', '-N'])
        self.popen.return_value.wait.assert_called_once_with(
            timeout=remote_transport.TUNNEL_STARTUP_CHECK_SECS
        )

    def test_open_tunnel_raises_if_ssh_exits_during_startup(self):
        tunnel = self.popen.return_value
        tunnel.wait.side_effect = None
        tunnel.returncode = 255
        with self.assertRaises(SenderSshError) as ctx:
            _ssh().open_tunnel(49573, 8000)
        msg = str(ctx.exception)
        self.assertIn('exited immediately (rc=255)', msg)
        self.assertIn('49573', msg)
        self.assertIn(INFRA_FAILURE_NOTE, msg)


class LocalTransportTest(fakes.SenderTestCase):
    def test_run_uses_shell(self):
        self.run.side_effect = None
        self.run.return_value = completed(stdout='hi')
        result = _ssh(host='localhost').run('echo hi')
        self.assertEqual(result.stdout, 'hi')
        self.assertEqual(self.run.call_args[0][0], 'echo hi')
        self.assertTrue(self.run.call_args.kwargs['shell'])

    def test_is_never_unreachable_and_has_no_tunnel(self):
        transport = _ssh(host=None)
        self.assertFalse(transport.known_unreachable)
        transport.raise_if_disconnected(completed(255), 'anything')
        with self.assertLogs(level='INFO'):
            self.assertIsNone(transport.open_tunnel(1, 2))
        self.popen.assert_not_called()


if __name__ == '__main__':
    unittest.main()
