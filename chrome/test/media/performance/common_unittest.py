#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for remote sender connectivity handling in common.py."""

import errno
import os
import socket
import subprocess
import sys
import types
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common  # pylint: disable=wrong-import-position

# pylint: disable=protected-access

_MAC_HOST = 'lin-137-e504--mac'
_WIN_HOST = 'lin-149-e504--win'
_USER = 'chrome-bot'
_IP = '10.0.0.37'
_NO_ROUTE = 'No route to host'
_SKIP_LOG = 'WARNING:root:Skipping remote cleanup: sender unreachable.'
_PUBKEY_DENIED = 'chrome-bot@lin-137-e504--mac: Permission denied (publickey).'

_WIN_CIM_CMD = (
    'powershell -Command "(Get-CimInstance Win32_Processor).Architecture"'
)
_WIN_ENV_CMD = (
    'powershell -Command "if ($env:PROCESSOR_ARCHITEW6432) '
    '{ $env:PROCESSOR_ARCHITEW6432 } else '
    '{ $env:PROCESSOR_ARCHITECTURE }"'
)
_WIN_VER_CMD = (
    'powershell -Command "[System.Environment]::OSVersion.Version.ToString()"'
)


def _make_args(sender=_MAC_HOST, username=_USER, sender_os='mac'):
    return types.SimpleNamespace(
        sender=sender, username=username, sender_os=sender_os
    )


def _addrinfo(ip=_IP):
    return [(socket.AF_INET, socket.SOCK_STREAM, 6, '', (ip, 22))]


def _done(returncode=0, stdout='', stderr=''):
    return subprocess.CompletedProcess(
        args=['ssh'], returncode=returncode, stdout=stdout, stderr=stderr
    )


def _no_route():
    return OSError(errno.EHOSTUNREACH, _NO_ROUTE)


class FakeSsh:
    """Stands in for subprocess.run and answers ssh commands by remote cmd."""

    def __init__(self):
        self.responses = {'echo ok': _done(stdout='ok\n')}
        self.default = _done()
        self.remote_commands = []

    def __call__(self, argv, **kwargs):
        del kwargs
        assert argv[0] == 'ssh', argv
        command = argv[-1]
        self.remote_commands.append(command)
        return self.responses.get(command, self.default)

    def ran(self, command):
        return command in self.remote_commands


class CommonTestCase(unittest.TestCase):
    """Patches sockets, subprocess.run, and sleep for every test."""

    def setUp(self):
        common._sender_preflight_results.clear()
        self.addCleanup(common._sender_preflight_results.clear)

        self.getaddrinfo = self._patch(
            'common.socket.getaddrinfo', return_value=_addrinfo()
        )
        self.conn = mock.MagicMock()
        self.create_connection = self._patch(
            'common.socket.create_connection', return_value=self.conn
        )
        self.ssh = FakeSsh()
        self.run = self._patch('common.subprocess.run', side_effect=self.ssh)
        self.popen = self._patch('common.subprocess.Popen')
        self.sleep = self._patch('common.time.sleep')

    def _patch(self, target, **kwargs):
        patcher = mock.patch(target, **kwargs)
        self.addCleanup(patcher.stop)
        return patcher.start()

    def _make_unreachable(self):
        self.create_connection.side_effect = _no_route()


class VerifySenderConnectivityTest(CommonTestCase):
    # Spec case 1.
    def test_dns_failure_raises_not_found_without_ssh(self):
        self.getaddrinfo.side_effect = socket.gaierror(
            socket.EAI_NONAME, 'Name or service not known'
        )

        with self.assertLogs(level='WARNING'):
            with self.assertRaises(common.SenderNotFoundError) as ctx:
                common.verify_sender_connectivity(_make_args())

        msg = str(ctx.exception)
        self.assertIn(f"Sender '{_MAC_HOST}' does not resolve", msg)
        self.assertIn('Name or service not known', msg)
        self.assertIn('--sender', msg)
        self.assertIn(common.INFRA_FAILURE_NOTE, msg)
        self.create_connection.assert_not_called()
        self.run.assert_not_called()

    # Spec case 2.
    def test_no_route_raises_unreachable_after_three_attempts(self):
        self._make_unreachable()

        with self.assertLogs(level='WARNING') as logs:
            with self.assertRaises(common.SenderUnreachableError) as ctx:
                common.verify_sender_connectivity(_make_args())

        self.assertEqual(self.create_connection.call_count, 3)
        self.create_connection.assert_called_with((_MAC_HOST, 22), timeout=5)
        self.assertEqual(self.sleep.call_args_list, [mock.call(5)] * 2)
        self.run.assert_not_called()

        reason = f'[Errno {errno.EHOSTUNREACH}] {_NO_ROUTE}'
        self.assertEqual(
            logs.output,
            [
                f'WARNING:root:Sender check attempt {i}/3 failed for '
                f'{_MAC_HOST}: {reason}'
                for i in (1, 2, 3)
            ],
        )
        self.assertEqual(
            str(ctx.exception),
            f"Sender '{_MAC_HOST}' ({_IP}) is not reachable on port 22: "
            f"{reason}. Likely causes: device powered off/asleep/rebooting, "
            "network link or DHCP lease lost, or sshd not running. "
            "This is an infra failure, not a test failure.",
        )

    def test_tcp_timeout_raises_unreachable(self):
        self.create_connection.side_effect = socket.timeout('timed out')
        with self.assertLogs(level='WARNING'):
            with self.assertRaises(common.SenderUnreachableError) as ctx:
                common.verify_sender_connectivity(_make_args())
        self.assertIn('timed out', str(ctx.exception))

    # Spec case 3.
    def test_ssh_255_raises_ssh_error_with_stderr(self):
        self.ssh.responses['echo ok'] = _done(255, stderr=_PUBKEY_DENIED)

        with self.assertLogs(level='WARNING') as logs:
            with self.assertRaises(common.SenderSshError) as ctx:
                common.verify_sender_connectivity(_make_args())

        msg = str(ctx.exception)
        self.assertIn('rc=255', msg)
        self.assertIn(_PUBKEY_DENIED, msg)
        self.assertIn(common.INFRA_FAILURE_NOTE, msg)
        # send_ssh_command itself also reports the 255 at ERROR.
        self.assertIn(
            f'ERROR:root:SSH to {_MAC_HOST} failed (rc=255): {_PUBKEY_DENIED}',
            logs.output,
        )

    def test_ssh_unexpected_stdout_raises_ssh_error(self):
        self.ssh.responses['echo ok'] = _done(stdout='Welcome!\n')
        with self.assertLogs(level='WARNING'):
            with self.assertRaises(common.SenderSshError) as ctx:
                common.verify_sender_connectivity(_make_args())
        self.assertIn('Welcome!', str(ctx.exception))

    def test_ssh_timeout_raises_ssh_error(self):
        self.run.side_effect = subprocess.TimeoutExpired(cmd='ssh', timeout=120)
        with self.assertLogs(level='WARNING'):
            with self.assertRaises(common.SenderSshError) as ctx:
                common.verify_sender_connectivity(_make_args())
        self.assertIn('timed out', str(ctx.exception))

    # Spec case 4.
    def test_healthy_sender_passes_and_is_cached(self):
        with self.assertLogs(level='INFO') as logs:
            common.verify_sender_connectivity(_make_args())
        common.verify_sender_connectivity(_make_args())

        self.assertEqual(
            logs.output,
            [f'INFO:root:Sender {_MAC_HOST} ({_IP}) reachable over SSH.'],
        )
        self.getaddrinfo.assert_called_once_with(_MAC_HOST, 22)
        self.create_connection.assert_called_once()
        self.conn.close.assert_called_once()
        self.assertEqual(self.ssh.remote_commands, ['echo ok'])
        self.sleep.assert_not_called()

    def test_accepts_windows_line_endings(self):
        self.ssh.responses['echo ok'] = _done(stdout='ok\r\n')
        common.verify_sender_connectivity(_make_args())

    # Spec case 5.
    def test_local_senders_run_no_checks(self):
        for sender in ('localhost', '127.0.0.1', None):
            with self.subTest(sender=sender):
                common.verify_sender_connectivity(_make_args(sender=sender))
        self.getaddrinfo.assert_not_called()
        self.create_connection.assert_not_called()
        self.run.assert_not_called()

    def test_recovers_when_sender_returns_mid_retry(self):
        self.create_connection.side_effect = [_no_route(), self.conn]
        with self.assertLogs(level='INFO') as logs:
            common.verify_sender_connectivity(_make_args())
        self.sleep.assert_called_once_with(5)
        self.assertTrue(logs.output[0].startswith('WARNING'))
        self.assertTrue(logs.output[-1].startswith('INFO'))

    def test_failure_is_cached_and_reraised_without_reprobing(self):
        self._make_unreachable()
        with self.assertLogs(level='WARNING'):
            with self.assertRaises(common.SenderUnreachableError):
                common.verify_sender_connectivity(_make_args())
        self.create_connection.reset_mock()
        self.sleep.reset_mock()

        with self.assertRaises(common.SenderUnreachableError):
            common.verify_sender_connectivity(_make_args())
        self.create_connection.assert_not_called()
        self.sleep.assert_not_called()

    def test_cache_is_keyed_on_host_and_user(self):
        common.verify_sender_connectivity(_make_args())
        common.verify_sender_connectivity(_make_args(username='other'))
        common.verify_sender_connectivity(_make_args(sender=_WIN_HOST))
        self.assertEqual(self.ssh.remote_commands, ['echo ok'] * 3)

    def test_unexpected_exceptions_are_not_swallowed(self):
        self.run.side_effect = ValueError('bug')
        with self.assertRaises(ValueError):
            common.verify_sender_connectivity(_make_args())
        self.sleep.assert_not_called()

    def test_exception_hierarchy(self):
        for cls in (
            common.SenderNotFoundError,
            common.SenderUnreachableError,
            common.SenderSshError,
        ):
            self.assertTrue(issubclass(cls, common.RemoteDeviceError))
            self.assertIn('infra failure, not a test failure', cls.__doc__)
        self.assertTrue(issubclass(common.RemoteDeviceError, RuntimeError))
        self.assertIn(
            'infra failure, not a test failure',
            common.RemoteDeviceError.__doc__,
        )


class SendSshCommandTest(CommonTestCase):
    def test_uses_batch_mode_and_connect_timeout(self):
        common.send_ssh_command(_MAC_HOST, _USER, 'true', blocking=True)
        argv = self.run.call_args[0][0]
        opts = list(zip(argv, argv[1:]))
        self.assertIn(('-o', 'StrictHostKeyChecking=no'), opts)
        self.assertIn(('-o', 'BatchMode=yes'), opts)
        self.assertIn(('-o', 'ConnectTimeout=10'), opts)

    def test_non_blocking_uses_same_options(self):
        common.send_ssh_command(_MAC_HOST, _USER, 'true')
        argv = self.popen.call_args[0][0]
        self.assertIn('BatchMode=yes', argv)
        self.assertIn('ConnectTimeout=10', argv)

    def test_logs_error_on_255_and_still_returns_result(self):
        self.ssh.default = _done(255, stderr=_NO_ROUTE)
        with self.assertLogs(level='ERROR') as logs:
            result = common.send_ssh_command(
                _MAC_HOST, _USER, 'true', blocking=True
            )
        self.assertEqual(result.returncode, 255)
        self.assertEqual(
            logs.output,
            [f'ERROR:root:SSH to {_MAC_HOST} failed (rc=255): {_NO_ROUTE}'],
        )

    def test_does_not_log_error_for_remote_command_failures(self):
        self.ssh.default = _done(1)
        with self.assertNoLogs(level='ERROR'):
            common.send_ssh_command(_MAC_HOST, _USER, 'false', blocking=True)


class GetRemoteInfoTest(CommonTestCase):
    def test_unreachable_mac_raises_before_uname(self):
        self._make_unreachable()
        with self.assertLogs(level='WARNING'):
            with self.assertRaises(common.SenderUnreachableError):
                common.get_remote_info(_make_args())
        self.run.assert_not_called()

    def test_mac_empty_uname_raises_with_raw_output(self):
        self.ssh.responses['/usr/bin/uname -m'] = _done(stderr='boom')
        self.ssh.responses['/usr/bin/sw_vers -productVersion'] = _done(
            stdout='14.5\n'
        )
        with self.assertRaises(common.RemoteDeviceError) as ctx:
            common.get_remote_info(_make_args())
        msg = str(ctx.exception)
        self.assertIn('arch', msg)
        self.assertIn(_MAC_HOST, msg)
        self.assertIn('sender_os=mac', msg)
        self.assertIn("'boom'", msg)
        self.assertNotIn('os_version', msg)

    def test_mac_healthy(self):
        self.ssh.responses['/usr/bin/uname -m'] = _done(stdout='arm64\n')
        self.ssh.responses['/usr/bin/sw_vers -productVersion'] = _done(
            stdout='14.5\n'
        )
        self.assertEqual(
            common.get_remote_info(_make_args()),
            {'arch': 'arm64', 'os_version': '14.5'},
        )

    # Spec case 6.
    def test_win_empty_probes_raise_instead_of_returning_x86(self):
        self.ssh.responses[_WIN_CIM_CMD] = _done(stdout='')
        self.ssh.responses[_WIN_ENV_CMD] = _done(stdout='')
        self.ssh.responses[_WIN_VER_CMD] = _done(stdout='10.0.22631.0\n')
        args = _make_args(sender=_WIN_HOST, sender_os='win')

        with self.assertRaises(common.RemoteDeviceError) as ctx:
            common.get_remote_info(args)

        msg = str(ctx.exception)
        self.assertIn(_WIN_HOST, msg)
        self.assertIn('sender_os=win', msg)
        # Both the CIM and the environment fallback probes are reported.
        self.assertIn('Get-CimInstance', msg)
        self.assertIn('PROCESSOR_ARCHITECTURE', msg)

    def test_win_unknown_arch_does_not_become_x86(self):
        self.ssh.responses[_WIN_CIM_CMD] = _done(stdout='')
        self.ssh.responses[_WIN_ENV_CMD] = _done(stdout='IA64\n')
        self.ssh.responses[_WIN_VER_CMD] = _done(stdout='10.0\n')
        with self.assertRaises(common.RemoteDeviceError):
            common.get_remote_info(
                _make_args(sender=_WIN_HOST, sender_os='win')
            )

    def test_win_fallback_maps_known_values(self):
        self.ssh.responses[_WIN_CIM_CMD] = _done(stdout='')
        self.ssh.responses[_WIN_VER_CMD] = _done(stdout='10.0\n')
        for raw, expected in (
            ('AMD64', 'x64'),
            ('ARM64', 'x64'),
            ('x86', 'x86'),
        ):
            with self.subTest(raw=raw):
                self.ssh.responses[_WIN_ENV_CMD] = _done(stdout=f'{raw}\r\n')
                info = common.get_remote_info(
                    _make_args(sender=_WIN_HOST, sender_os='win')
                )
                self.assertEqual(info['arch'], expected)


class TerminateOldChromedriverTest(CommonTestCase):
    def _check_cmd(self):
        return common.SENDER_CHROMEDRIVER_CHECK_CMD['mac']

    # Spec case 7.
    def test_255_raises_and_does_not_report_gone(self):
        self.ssh.responses[self._check_cmd()] = _done(255, stderr=_NO_ROUTE)

        with self.assertLogs(level='INFO') as logs:
            with self.assertRaises(common.SenderUnreachableError) as ctx:
                common.terminate_old_chromedriver(_make_args())

        self.assertIn(_NO_ROUTE, str(ctx.exception))
        self.assertFalse(any('confirmed gone' in l for l in logs.output))
        # Later cleanup must see the sender as unreachable.
        self.assertTrue(common._sender_check_failed(_make_args()))

    def test_255_on_kill_command_raises(self):
        kill_cmd = common.SENDER_TERMINATE_DRIVER_CMD['mac']
        self.ssh.responses[kill_cmd] = _done(255, stderr=_NO_ROUTE)
        with self.assertLogs(level='ERROR'):
            with self.assertRaises(common.SenderUnreachableError):
                common.terminate_old_chromedriver(_make_args())
        self.assertFalse(self.ssh.ran(self._check_cmd()))

    def test_grep_exit_1_means_no_processes(self):
        self.ssh.responses[self._check_cmd()] = _done(1, stdout='')
        with self.assertLogs(level='INFO') as logs:
            common.terminate_old_chromedriver(_make_args())
        self.assertTrue(any('confirmed gone' in l for l in logs.output))

    def test_unreachable_sender_raises_before_any_ssh(self):
        self._make_unreachable()
        with self.assertLogs(level='WARNING'):
            with self.assertRaises(common.SenderUnreachableError):
                common.terminate_old_chromedriver(_make_args())
        self.run.assert_not_called()


class SetupEntryPointsTest(CommonTestCase):
    """The acceptance scenario: stop before uname, CfT download, or setup."""

    def test_install_and_setup_chrome_stops_before_cft_download(self):
        self._make_unreachable()
        with mock.patch('common.download_cft_urls') as download:
            with self.assertLogs(level='WARNING'):
                with self.assertRaises(common.SenderUnreachableError) as ctx:
                    common.install_and_setup_chrome(_make_args(), None)
            download.assert_not_called()
        self.run.assert_not_called()
        self.assertNotIn('Unsupported OS/Arch', str(ctx.exception))

    def test_setup_cros_environment_stops_before_crossbench(self):
        self._make_unreachable()
        with mock.patch('common.download_cft_urls') as download:
            with self.assertLogs(level='WARNING'):
                with self.assertRaises(common.SenderUnreachableError):
                    common.setup_cros_environment(
                        _make_args(sender_os='cros'), None, []
                    )
            download.assert_not_called()
        self.run.assert_not_called()

    def test_start_ssh_tunnel_does_not_spawn_tunnel(self):
        self._make_unreachable()
        with self.assertLogs(level='WARNING'):
            with self.assertRaises(common.SenderUnreachableError):
                common.start_ssh_tunnel(_make_args())
        self.popen.assert_not_called()


class CleanupTest(CommonTestCase):
    def _fail_preflight(self):
        self._make_unreachable()
        with self.assertLogs(level='WARNING'):
            with self.assertRaises(common.SenderUnreachableError):
                common.verify_sender_connectivity(_make_args())
        self.run.reset_mock()

    def test_cleanup_binaries_skips_when_sender_unreachable(self):
        self._fail_preflight()
        with self.assertLogs(level='INFO') as logs:
            common.cleanup_binaries(_make_args())
        self.run.assert_not_called()
        self.assertIn(_SKIP_LOG, logs.output)
        self.assertFalse(
            any('Cleaned up remote' in l for l in logs.output), logs.output
        )
        self.assertFalse(any('confirmed gone' in l for l in logs.output))

    def test_cleanup_binaries_skips_when_terminate_finds_255(self):
        check_cmd = common.SENDER_CHROMEDRIVER_CHECK_CMD['mac']
        self.ssh.responses[check_cmd] = _done(255, stderr=_NO_ROUTE)
        with self.assertLogs(level='INFO') as logs:
            common.cleanup_binaries(_make_args())
        self.assertFalse(self.ssh.ran('rm -rf /tmp/chrome* /tmp/chromedriver*'))
        self.assertIn(_SKIP_LOG, logs.output)

    def test_cleanup_binaries_warns_on_nonzero_rc(self):
        self.ssh.responses['rm -rf /tmp/chrome* /tmp/chromedriver*'] = _done(
            1, stderr='rm: permission denied'
        )
        with self.assertLogs(level='INFO') as logs:
            common.cleanup_binaries(_make_args())
        self.assertFalse(any('Cleaned up remote' in l for l in logs.output))
        self.assertTrue(
            any(
                l.startswith('WARNING')
                and 'rc=1' in l
                and 'permission denied' in l
                for l in logs.output
            ),
            logs.output,
        )

    def test_cleanup_binaries_logs_success_on_rc_0(self):
        with self.assertLogs(level='INFO') as logs:
            common.cleanup_binaries(_make_args())
        self.assertIn(
            'INFO:root:Cleaned up remote Chrome/Chromedriver directories.',
            logs.output,
        )

    def test_cleanup_binaries_never_raises(self):
        common.verify_sender_connectivity(_make_args())
        self.run.side_effect = subprocess.TimeoutExpired(cmd='ssh', timeout=120)
        with self.assertLogs(level='WARNING'):
            common.cleanup_binaries(_make_args())

    def test_teardown_skips_remote_cleanup_when_unreachable(self):
        self._fail_preflight()
        with self.assertLogs(level='WARNING') as logs:
            common.teardown_test_environment(None, None, _make_args())
        self.run.assert_not_called()
        self.assertIn(_SKIP_LOG, logs.output)

    def test_teardown_warns_on_nonzero_rc(self):
        self.ssh.responses['rm -f /tmp/*.zip'] = _done(255, stderr=_NO_ROUTE)
        with self.assertLogs(level='INFO') as logs:
            common.teardown_test_environment(None, None, _make_args())
        self.assertFalse(
            any('Cleaned up tmp files' in l for l in logs.output), logs.output
        )
        self.assertTrue(any('rc=255' in l for l in logs.output))

    def test_teardown_never_raises(self):
        self.run.side_effect = subprocess.TimeoutExpired(cmd='ssh', timeout=120)
        with self.assertLogs(level='WARNING'):
            common.teardown_test_environment(None, None, _make_args())


if __name__ == '__main__':
    unittest.main()
