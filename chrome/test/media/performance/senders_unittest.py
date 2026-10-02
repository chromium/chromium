#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for senders.py."""

import unittest
from unittest import mock

import perf_test_fakes as fakes
from perf_test_fakes import MAC_HOST, NO_ROUTE, WIN_HOST, completed, make_args
from remote_transport import RemoteDeviceError, SenderUnreachableError
import senders

_WIN_CIM_CMD = ('powershell -Command '
                '"(Get-CimInstance Win32_Processor).Architecture"')
_WIN_ENV_CMD = ('powershell -Command "if ($env:PROCESSOR_ARCHITEW6432) '
                '{ $env:PROCESSOR_ARCHITEW6432 } else '
                '{ $env:PROCESSOR_ARCHITECTURE }"')
_WIN_VER_CMD = ('powershell -Command '
                '"[System.Environment]::OSVersion.Version.ToString()"')
_MAC_UNAME = '/usr/bin/uname -m'
_MAC_SW_VERS = '/usr/bin/sw_vers -productVersion'
_MAC_CHECK = senders.MacSender.CHROMEDRIVER_CHECK_CMD
_MAC_KILL = senders.MacSender.TERMINATE_CHROMEDRIVER_CMD
_MAC_RM_BINARIES = 'rm -rf /tmp/chrome* /tmp/chromedriver*'

_CFT_BASE = 'https://storage.example/120.0.1.2'
_CHROME_URL = f'{_CFT_BASE}/mac-arm64/chrome-mac-arm64.zip'
_DRIVER_URL = f'{_CFT_BASE}/mac-arm64/chromedriver-mac-arm64.zip'


def _mac(**kwargs):
    return senders.make_sender(make_args(**kwargs))


def _win():
    return senders.make_sender(make_args(sender=WIN_HOST, sender_os='win'))


class MakeSenderTest(unittest.TestCase):

    def test_registry_covers_every_sender_os(self):
        self.assertEqual(
            {
                os_name: cls.__name__
                for os_name, cls in senders.SENDER_CLASSES.items()
            }, {
                'mac': 'MacSender',
                'linux': 'LinuxSender',
                'cros': 'CrosSender',
                'win': 'WindowsSender',
            })

    def test_unknown_os_raises(self):
        with self.assertRaises(NotImplementedError):
            senders.make_sender(make_args(sender_os='beos'))

    def test_tmp_dirs(self):
        self.assertEqual(senders.MacSender.TMP_DIR, '/tmp')
        self.assertEqual(senders.LinuxSender.TMP_DIR, '/tmp')
        self.assertEqual(senders.CrosSender.TMP_DIR, '/usr/local/tmp')
        self.assertEqual(senders.WindowsSender.TMP_DIR, 'C:/cft_temp')


class DetectPlatformTest(fakes.SenderTestCase):

    def test_unreachable_mac_raises_before_uname(self):
        self.make_unreachable()
        with self.assertLogs(level='WARNING'):
            with self.assertRaises(SenderUnreachableError):
                _mac().detect_platform()
        self.run.assert_not_called()

    def test_mac_healthy(self):
        self.ssh.responses[_MAC_UNAME] = completed(stdout='arm64\n')
        self.ssh.responses[_MAC_SW_VERS] = completed(stdout='14.5\n')
        self.assertEqual(_mac().detect_platform(), {
            'arch': 'arm64',
            'os_version': '14.5'
        })

    def test_mac_empty_uname_raises_with_raw_output(self):
        self.ssh.responses[_MAC_UNAME] = completed(stderr='boom')
        self.ssh.responses[_MAC_SW_VERS] = completed(stdout='14.5\n')
        with self.assertRaises(RemoteDeviceError) as ctx:
            _mac().detect_platform()
        msg = str(ctx.exception)
        self.assertIn('arch', msg)
        self.assertIn(MAC_HOST, msg)
        self.assertIn('sender_os=mac', msg)
        self.assertIn("'boom'", msg)
        self.assertNotIn('os_version', msg)

    def test_linux_maps_x86_64(self):
        self.ssh.responses['uname -m'] = completed(stdout='x86_64\n')
        self.ssh.responses['uname -r'] = completed(stdout='6.1.0\n')
        self.assertEqual(
            _mac(sender_os='linux').detect_platform(), {
                'arch': 'x64',
                'os_version': '6.1.0'
            })

    def test_cros_uses_fixed_os_version(self):
        self.ssh.responses['/usr/bin/uname -m'] = completed(stdout='x86_64\n')
        self.assertEqual(
            _mac(sender_os='cros').detect_platform(), {
                'arch': 'x64',
                'os_version': 'cros'
            })

    def test_win_cim_codes(self):
        self.ssh.responses[_WIN_VER_CMD] = completed(stdout='10.0\n')
        for code, expected in (('9', 'x64'), ('12', 'x64'), ('0', 'x86')):
            with self.subTest(code=code):
                self.ssh.responses[_WIN_CIM_CMD] = completed(stdout=code)
                self.assertEqual(_win().detect_platform()['arch'], expected)
        self.assertFalse(self.ssh.ran(_WIN_ENV_CMD))

    # Spec case 6.
    def test_win_empty_probes_raise_instead_of_returning_x86(self):
        self.ssh.responses[_WIN_CIM_CMD] = completed(stdout='')
        self.ssh.responses[_WIN_ENV_CMD] = completed(stdout='')
        self.ssh.responses[_WIN_VER_CMD] = completed(stdout='10.0.22631.0\n')

        with self.assertRaises(RemoteDeviceError) as ctx:
            _win().detect_platform()

        msg = str(ctx.exception)
        self.assertIn(WIN_HOST, msg)
        self.assertIn('sender_os=win', msg)
        # Both the CIM and the environment fallback probes are reported.
        self.assertIn('Get-CimInstance', msg)
        self.assertIn('PROCESSOR_ARCHITECTURE', msg)

    def test_win_unknown_arch_does_not_become_x86(self):
        self.ssh.responses[_WIN_CIM_CMD] = completed(stdout='')
        self.ssh.responses[_WIN_ENV_CMD] = completed(stdout='IA64\n')
        self.ssh.responses[_WIN_VER_CMD] = completed(stdout='10.0\n')
        with self.assertRaises(RemoteDeviceError):
            _win().detect_platform()

    def test_win_fallback_maps_known_values(self):
        self.ssh.responses[_WIN_CIM_CMD] = completed(stdout='')
        self.ssh.responses[_WIN_VER_CMD] = completed(stdout='10.0\n')
        for raw, expected in (('AMD64', 'x64'), ('ARM64', 'x64'), ('x86',
                                                                   'x86')):
            with self.subTest(raw=raw):
                self.ssh.responses[_WIN_ENV_CMD] = completed(
                    stdout=f'{raw}\r\n')
                self.assertEqual(_win().detect_platform()['arch'], expected)

    def test_local_sender_uses_platform_module(self):
        with mock.patch('senders.platform.machine', return_value='x86_64'), \
             mock.patch('senders.platform.release', return_value='6.1'):
            info = _mac(sender='localhost').detect_platform()
        self.assertEqual(info, {'arch': 'x64', 'os_version': '6.1'})
        self.run.assert_not_called()


class TerminateChromedriverTest(fakes.SenderTestCase):

    # Spec case 7.
    def test_255_raises_and_does_not_report_gone(self):
        self.ssh.responses[_MAC_CHECK] = completed(255, stderr=NO_ROUTE)
        sender = _mac()

        with self.assertLogs(level='INFO') as logs:
            with self.assertRaises(SenderUnreachableError) as ctx:
                sender.terminate_chromedriver()

        self.assertIn(NO_ROUTE, str(ctx.exception))
        self.assertFalse(any('confirmed gone' in l for l in logs.output))
        # Later cleanup must see the sender as unreachable.
        self.assertTrue(_mac().transport.known_unreachable)

    def test_255_on_kill_command_raises(self):
        self.ssh.responses[_MAC_KILL] = completed(255, stderr=NO_ROUTE)
        with self.assertLogs(level='ERROR'):
            with self.assertRaises(SenderUnreachableError):
                _mac().terminate_chromedriver()
        self.assertFalse(self.ssh.ran(_MAC_CHECK))

    def test_grep_exit_1_means_no_processes(self):
        self.ssh.responses[_MAC_CHECK] = completed(1, stdout='')
        with self.assertLogs(level='INFO') as logs:
            _mac().terminate_chromedriver()
        self.assertTrue(any('confirmed gone' in l for l in logs.output))

    def test_unreachable_sender_raises_before_any_ssh(self):
        self.make_unreachable()
        with self.assertLogs(level='WARNING'):
            with self.assertRaises(SenderUnreachableError):
                _mac().terminate_chromedriver()
        self.run.assert_not_called()

    def test_lingering_processes_raise(self):
        self.ssh.responses[_MAC_CHECK] = completed(stdout='123 chromedriver')
        with self.assertLogs(level='INFO'):
            with self.assertRaises(RuntimeError):
                _mac().terminate_chromedriver()
        self.assertEqual(self.ssh.remote_commands.count(_MAC_CHECK), 15)


class WaitForChromedriverTest(fakes.SenderTestCase):

    def test_ready(self):
        self.ssh.responses[senders.MacSender.STATUS_CMD] = completed(
            stdout='200')
        with self.assertLogs(level='INFO') as logs:
            _mac().wait_for_chromedriver()
        self.assertIn('INFO:root:Chromedriver is ready.', logs.output)

    def test_never_ready_dumps_logs_and_raises(self):
        self.ssh.responses[senders.MacSender.STATUS_CMD] = completed(
            stdout='000')
        log_cmd = 'cat /tmp/chromedriver_console*.log 2>/dev/null || true'
        self.ssh.responses[log_cmd] = completed(stdout='driver crashed')
        with self.assertLogs(level='WARNING') as logs:
            with self.assertRaises(RuntimeError):
                _mac().wait_for_chromedriver()
        self.assertTrue(any('driver crashed' in l for l in logs.output))


class InstallChromeTest(fakes.SenderTestCase):

    def setUp(self):
        super().setUp()
        self.download = self._patch('senders.download_cft_urls',
                                    return_value=('120.0.1.2', _CHROME_URL,
                                                  _DRIVER_URL))
        self.ssh.responses[_MAC_UNAME] = completed(stdout='arm64\n')
        self.ssh.responses[_MAC_SW_VERS] = completed(stdout='14.5\n')

    def test_unreachable_stops_before_cft_download(self):
        self.make_unreachable()
        with self.assertLogs(level='WARNING'):
            with self.assertRaises(SenderUnreachableError) as ctx:
                _mac().install_chrome(None)
        self.download.assert_not_called()
        self.run.assert_not_called()
        self.assertNotIn('Unsupported OS/Arch', str(ctx.exception))

    def test_unsupported_arch_raises(self):
        self.ssh.responses[_MAC_UNAME] = completed(stdout='ppc\n')
        with self.assertRaises(NotImplementedError) as ctx:
            _mac().install_chrome(None)
        self.assertIn('mac/ppc', str(ctx.exception))
        self.download.assert_not_called()

    def test_mac_downloads_when_missing_and_starts_chromedriver(self):
        self.ssh.default = completed(stdout='MISSING')
        app_path, version = _mac().install_chrome('120')

        self.download.assert_called_once_with('mac-arm64', '120')
        self.assertEqual(version, '120.0.1.2')
        self.assertEqual(
            app_path, '/tmp/chrome-mac-arm64/Google Chrome for Testing.app')
        commands = self.ssh.remote_commands
        self.assertTrue(any(c.startswith('curl -L') for c in commands))
        self.assertIn(
            'xattr -cr /tmp/chrome-mac-arm64 && '
            'xattr -cr /tmp/chromedriver-mac-arm64', commands)
        self.assertIn('chmod +x /tmp/chromedriver-mac-arm64/chromedriver',
                      commands)
        spawned = self.popen.call_args[0][0][-1]
        self.assertTrue(
            spawned.startswith(
                'nohup /tmp/chromedriver-mac-arm64/chromedriver --port='))
        self.assertIn('--allowed-origins="*"', spawned)

    def test_mac_skips_download_when_installed(self):
        self.ssh.default = completed(stdout='EXISTS')
        _mac().install_chrome('120')
        self.assertFalse(
            any(c.startswith('curl -L') for c in self.ssh.remote_commands))
        self.popen.assert_called_once()

    def test_cros_does_not_start_chromedriver(self):
        self.ssh.responses['/usr/bin/uname -m'] = completed(stdout='x86_64\n')
        self.ssh.default = completed(stdout='EXISTS')
        app_path, _ = _mac(sender_os='cros').install_chrome('120')
        self.assertTrue(app_path.startswith('/usr/local/tmp/'))
        self.download.assert_called_once_with('linux64', '120')
        self.popen.assert_not_called()

    def test_windows_downloads_when_missing_and_starts_chromedriver(self):
        win_chrome_url = f'{_CFT_BASE}/win64/chrome-win64.zip'
        win_driver_url = f'{_CFT_BASE}/win64/chromedriver-win64.zip'
        self.download.return_value = ('120.0.1.2', win_chrome_url,
                                      win_driver_url)
        self.ssh.responses[_WIN_CIM_CMD] = completed(stdout='9')
        self.ssh.responses[_WIN_VER_CMD] = completed(stdout='10.0\n')
        self.ssh.default = completed(stdout='MISSING')
        app_path, version = _win().install_chrome('120')

        self.download.assert_called_once_with('win64', '120')
        self.assertEqual(version, '120.0.1.2')
        self.assertEqual(app_path, 'C:/cft_temp/chrome-win64/chrome.exe')
        commands = self.ssh.remote_commands
        self.assertTrue(
            any("New-Item -ItemType Directory -Path 'C:/cft_temp'" in c
                for c in commands))

    def test_windows_setup_failure_raises(self):
        self.ssh.responses[_WIN_CIM_CMD] = completed(stdout='9')
        self.ssh.responses[_WIN_VER_CMD] = completed(stdout='10.0\n')
        self.ssh.default = completed(1, stdout='MISSING', stderr='bad zip')
        with self.assertRaises(RuntimeError) as ctx:
            _win().install_chrome('120')
        self.assertIn('bad zip', str(ctx.exception))
        self.download.assert_called_once_with('win64', '120')


class CleanupTest(fakes.SenderTestCase):

    def test_cleanup_binaries_skips_when_sender_unreachable(self):
        self.fail_preflight()
        with self.assertLogs(level='INFO') as logs:
            _mac().cleanup_binaries()
        self.run.assert_not_called()
        self.assertIn(fakes.SKIP_LOG, logs.output)
        self.assertFalse(any('Cleaned up remote' in l for l in logs.output),
                         logs.output)
        self.assertFalse(any('confirmed gone' in l for l in logs.output))

    def test_cleanup_binaries_skips_when_terminate_finds_255(self):
        self.ssh.responses[_MAC_CHECK] = completed(255, stderr=NO_ROUTE)
        with self.assertLogs(level='INFO') as logs:
            _mac().cleanup_binaries()
        self.assertFalse(self.ssh.ran(_MAC_RM_BINARIES))
        self.assertIn(fakes.SKIP_LOG, logs.output)

    def test_cleanup_binaries_warns_on_nonzero_rc(self):
        self.ssh.responses[_MAC_RM_BINARIES] = completed(
            1, stderr='rm: permission denied')
        with self.assertLogs(level='INFO') as logs:
            _mac().cleanup_binaries()
        self.assertFalse(any('Cleaned up remote' in l for l in logs.output))
        self.assertTrue(
            any(
                l.startswith('WARNING') and 'rc=1' in l
                and 'permission denied' in l
                for l in logs.output), logs.output)

    def test_cleanup_binaries_logs_success_on_rc_0(self):
        with self.assertLogs(level='INFO') as logs:
            _mac().cleanup_binaries()
        self.assertIn(
            'INFO:root:Cleaned up remote Chrome/Chromedriver directories.',
            logs.output)

    def test_cleanup_binaries_never_raises(self):
        _mac().verify_connectivity()
        self.run.side_effect = TimeoutError('ssh hung')
        with self.assertLogs(level='WARNING'):
            _mac().cleanup_binaries()

    def test_cleanup_commands_per_os(self):
        expected = {
            'mac': ('rm -f /tmp/*.zip', _MAC_RM_BINARIES),
            'linux': ('rm -f /tmp/*.zip', _MAC_RM_BINARIES),
            'cros':
            ('rm -f /usr/local/tmp/*.zip', 'rm -rf /usr/local/tmp/chrome* '
             '/usr/local/tmp/chromedriver*'),
            'win': ('powershell -Command "Remove-Item -Path C:/cft_temp/*.zip '
                    '-Force -ErrorAction SilentlyContinue"',
                    'powershell -Command "Remove-Item -Path '
                    'C:/cft_temp/chrome*,C:/cft_temp/chromedriver* '
                    '-Recurse -Force -ErrorAction SilentlyContinue"'),
        }
        for sender_os, (downloads, binaries) in expected.items():
            with self.subTest(sender_os=sender_os):
                sender = senders.make_sender(make_args(sender_os=sender_os))
                # pylint: disable=protected-access
                self.assertEqual(sender._downloads_cleanup_command(),
                                 downloads)
                self.assertEqual(sender._binaries_cleanup_command(), binaries)
                # pylint: enable=protected-access


class MonitoringTest(fakes.SenderTestCase):

    def test_glances_start_and_stop(self):
        sender = _mac()
        sender.start_monitoring('/tmp/g.csv')
        spawned = self.popen.call_args[0][0][-1]
        self.assertEqual(
            spawned, 'python3 -m glances -t 1 --export csv '
            '--export-csv-file /tmp/g.csv --quiet')

        proc = mock.MagicMock()
        sender.stop_monitoring(proc, '/tmp/g.csv', '/out/g.csv')
        proc.terminate.assert_called_once()
        self.assertEqual(self.ssh.remote_commands,
                         ['pkill -f glances', 'rm -f /tmp/g.csv'])
        self.assertEqual(self.ssh.copies[0][-2:],
                         [f'{fakes.USER}@{MAC_HOST}:/tmp/g.csv', '/out/g.csv'])

    def test_cros_uses_power_log(self):
        sender = _mac(sender_os='cros')
        sender.start_monitoring('/tmp/g.csv')
        self.assertIn('power_now', self.popen.call_args[0][0][-1])

        sender.stop_monitoring(None, '/tmp/g.csv', '/out/g.csv')
        self.assertIn('rm -f /tmp/cros_power.txt', self.ssh.remote_commands)
        self.assertEqual(self.ssh.copies[0][-1], '/out/g.csv')


if __name__ == '__main__':
    unittest.main()
