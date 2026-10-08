#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for senders.py."""

import subprocess
import unittest
from unittest import mock

import perf_test_fakes as fakes
from perf_test_fakes import MAC_HOST, NO_ROUTE, WIN_HOST, completed, make_args
from remote_transport import RemoteDeviceError, SenderUnreachableError
import senders

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
_MAC_UNAME = '/usr/bin/uname -m'
_MAC_SW_VERS = '/usr/bin/sw_vers -productVersion'
_MAC_CHECK = senders.MacSender.CHROMEDRIVER_CHECK_CMD
_MAC_KILL = senders.MacSender.TERMINATE_CHROMEDRIVER_CMD
_MAC_RM_BINARIES = 'rm -rf /tmp/chrome* /tmp/chromedriver*'

_CFT_BASE = 'https://storage.example/120.0.1.2'
_CHROME_URL = f'{_CFT_BASE}/mac-arm64/chrome-mac-arm64.zip'
_DRIVER_URL = f'{_CFT_BASE}/mac-arm64/chromedriver-mac-arm64.zip'
_MAC_APP = '/tmp/chrome-mac-arm64/Google Chrome for Testing.app'
_MAC_DRIVER = '/tmp/chromedriver-mac-arm64/chromedriver'
_MAC_INSTALLED_CHECK = (
    f"[ -d '{_MAC_APP}' ] && [ -f '{_MAC_DRIVER}' ] && "
    "echo 'EXISTS' || echo 'MISSING'"
)
_MAC_XATTR = (
    'xattr -cr /tmp/chrome-mac-arm64 && xattr -cr /tmp/chromedriver-mac-arm64'
)
_MAC_CHMOD = f'chmod +x {_MAC_DRIVER}'

_WIN_CHROME_URL = f'{_CFT_BASE}/win64/chrome-win64.zip'
_WIN_DRIVER_URL = f'{_CFT_BASE}/win64/chromedriver-win64.zip'
_WIN_APP = 'C:/cft_temp/chrome-win64/chrome.exe'
_WIN_DRIVER = 'C:/cft_temp/chromedriver-win64/chromedriver.exe'
_WIN_MKDIR = (
    "powershell -Command \"if (!(Test-Path 'C:/cft_temp')) "
    "{ New-Item -ItemType Directory -Path 'C:/cft_temp' -Force }\""
)
_WIN_INSTALLED_CHECK = (
    f"powershell -Command \"if ((Test-Path '{_WIN_APP}') -and "
    f"(Test-Path '{_WIN_DRIVER}')) "
    "{ Write-Output 'EXISTS' } else { Write-Output 'MISSING' }\""
)
_WIN_SETUP_PREFIX = 'powershell -Command "Set-Variable -Name ErrorAction'
_WIN_TASK_DELETE = (
    'powershell -Command "schtasks /delete /tn StartChromeDriverTask /f"'
)
_WIN_TASK_CREATE = (
    'powershell -Command "schtasks /create /tn '
    "StartChromeDriverTask /tr "
    "'C:/cft_temp/start_chromedriver.bat' /sc ONCE /st 23:59 "
    '/IT /f"'
)
_WIN_TASK_RUN = 'powershell -Command "schtasks /run /tn StartChromeDriverTask"'
_MISSING = completed(stdout='MISSING\n')
_EXISTS = completed(stdout='EXISTS\n')


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
            },
            {
                'mac': 'MacSender',
                'linux': 'LinuxSender',
                'cros': 'CrosSender',
                'win': 'WindowsSender',
            },
        )

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
        self.assertEqual(
            _mac().detect_platform(), {'arch': 'arm64', 'os_version': '14.5'}
        )

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
            _mac(sender_os='linux').detect_platform(),
            {'arch': 'x64', 'os_version': '6.1.0'},
        )

    def test_cros_uses_fixed_os_version(self):
        self.ssh.responses['/usr/bin/uname -m'] = completed(stdout='x86_64\n')
        self.assertEqual(
            _mac(sender_os='cros').detect_platform(),
            {'arch': 'x64', 'os_version': 'cros'},
        )

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
        for raw, expected in (
            ('AMD64', 'x64'),
            ('ARM64', 'x64'),
            ('x86', 'x86'),
        ):
            with self.subTest(raw=raw):
                self.ssh.responses[_WIN_ENV_CMD] = completed(
                    stdout=f'{raw}\r\n'
                )
                self.assertEqual(_win().detect_platform()['arch'], expected)

    def test_local_sender_uses_platform_module(self):
        with (
            mock.patch('senders.platform.machine', return_value='x86_64'),
            mock.patch('senders.platform.release', return_value='6.1'),
        ):
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
            stdout='200'
        )
        with self.assertLogs(level='INFO') as logs:
            _mac().wait_for_chromedriver()
        self.assertIn('INFO:root:Chromedriver is ready.', logs.output)

    def test_never_ready_dumps_logs_and_raises(self):
        self.ssh.responses[senders.MacSender.STATUS_CMD] = completed(
            stdout='000'
        )
        log_cmd = 'cat /tmp/chromedriver_console*.log 2>/dev/null || true'
        self.ssh.responses[log_cmd] = completed(stdout='driver crashed')
        with self.assertLogs(level='WARNING') as logs:
            with self.assertRaises(RuntimeError):
                _mac().wait_for_chromedriver()
        self.assertTrue(any('driver crashed' in l for l in logs.output))


class InstallChromeTest(fakes.SenderTestCase):
    def setUp(self):
        super().setUp()
        self.download = self._patch(
            'senders.download_cft_urls',
            return_value=('120.0.1.2', _CHROME_URL, _DRIVER_URL),
        )
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
        self.ssh.responses[_MAC_INSTALLED_CHECK] = [_MISSING, _EXISTS]
        sender = _mac()
        app_path, version = sender.install_chrome('120')

        self.download.assert_called_once_with('mac-arm64', '120')
        self.assertEqual(version, '120.0.1.2')
        self.assertEqual(app_path, _MAC_APP)
        self.assertEqual(sender.driver_path, _MAC_DRIVER)
        self.assertTrue(self.ssh.ran_prefix(f'curl -fL {_CHROME_URL} '))
        self.assertTrue(self.ssh.ran(_MAC_XATTR))
        self.assertTrue(self.ssh.ran(_MAC_CHMOD))
        spawned = self.popen.call_args[0][0][-1]
        self.assertTrue(spawned.startswith(f'nohup {_MAC_DRIVER} --port='))
        self.assertIn('--allowed-origins="*"', spawned)

    def test_mac_skips_download_when_installed(self):
        self.ssh.responses[_MAC_INSTALLED_CHECK] = _EXISTS
        sender = _mac()
        sender.install_chrome('120')
        self.assertFalse(self.ssh.ran_prefix('curl'))
        self.assertEqual(sender.driver_path, _MAC_DRIVER)
        self.popen.assert_called_once()

    def test_mac_download_failure_raises_with_stderr(self):
        self.ssh.responses[_MAC_INSTALLED_CHECK] = _MISSING
        self.ssh.default = completed(
            22, stderr='curl: (22) The requested URL returned error: 404'
        )
        with self.assertRaises(RuntimeError) as ctx:
            _mac().install_chrome('120')
        msg = str(ctx.exception)
        self.assertIn('downloading and unzipping Chrome', msg)
        self.assertIn('rc=22', msg)
        self.assertIn('error: 404', msg)
        self.assertFalse(self.ssh.ran(_MAC_XATTR))
        self.popen.assert_not_called()

    def test_mac_ssh_drop_during_download_is_infra_failure(self):
        self.ssh.responses[_MAC_INSTALLED_CHECK] = _MISSING
        self.ssh.default = completed(255, stderr=NO_ROUTE)
        with self.assertLogs(level='ERROR'):
            with self.assertRaises(SenderUnreachableError):
                _mac().install_chrome('120')

    def test_mac_missing_after_install_raises(self):
        self.ssh.responses[_MAC_INSTALLED_CHECK] = _MISSING
        with self.assertRaises(RuntimeError) as ctx:
            _mac().install_chrome('120')
        self.assertIn('missing', str(ctx.exception))
        self.assertIn(_MAC_APP, str(ctx.exception))
        self.popen.assert_not_called()

    def test_mac_xattr_failure_raises(self):
        self.ssh.responses[_MAC_INSTALLED_CHECK] = [_MISSING, _EXISTS]
        self.ssh.responses[_MAC_XATTR] = completed(1, stderr='xattr: denied')
        with self.assertRaises(RuntimeError) as ctx:
            _mac().install_chrome('120')
        self.assertIn('xattr: denied', str(ctx.exception))
        self.popen.assert_not_called()

    def test_chmod_failure_raises_before_spawning(self):
        self.ssh.responses[_MAC_INSTALLED_CHECK] = _EXISTS
        self.ssh.responses[_MAC_CHMOD] = completed(1, stderr='read-only')
        with self.assertRaises(RuntimeError) as ctx:
            _mac().install_chrome('120')
        self.assertIn('read-only', str(ctx.exception))
        self.popen.assert_not_called()

    def test_cros_does_not_start_chromedriver(self):
        self.ssh.responses['/usr/bin/uname -m'] = completed(stdout='x86_64\n')
        self.ssh.default = _EXISTS
        sender = _mac(sender_os='cros')
        app_path, _ = sender.install_chrome('120')
        self.assertTrue(app_path.startswith('/usr/local/tmp/'))
        self.assertEqual(
            sender.driver_path,
            '/usr/local/tmp/chromedriver-mac-arm64/chromedriver',
        )
        self.download.assert_called_once_with('linux64', '120')
        self.popen.assert_not_called()


class InstallChromeWindowsTest(fakes.SenderTestCase):
    def setUp(self):
        super().setUp()
        self.download = self._patch(
            'senders.download_cft_urls',
            return_value=('120.0.1.2', _WIN_CHROME_URL, _WIN_DRIVER_URL),
        )
        self.ssh.responses[_WIN_CIM_CMD] = completed(stdout='9')
        self.ssh.responses[_WIN_VER_CMD] = completed(stdout='10.0\n')
        self.ssh.responses[_WIN_INSTALLED_CHECK] = [_MISSING, _EXISTS]

    def test_installs_and_schedules_chromedriver(self):
        sender = _win()
        app_path, _ = sender.install_chrome('120')

        self.download.assert_called_once_with('win64', '120')
        self.assertEqual(app_path, _WIN_APP)
        self.assertEqual(sender.driver_path, _WIN_DRIVER)
        self.assertTrue(self.ssh.ran(_WIN_MKDIR))
        setup = [
            c
            for c in self.ssh.remote_commands
            if c.startswith(_WIN_SETUP_PREFIX)
        ]
        self.assertEqual(len(setup), 1)
        self.assertIn(f"curl.exe -fL '{_WIN_CHROME_URL}'", setup[0])
        for command in (_WIN_TASK_DELETE, _WIN_TASK_CREATE, _WIN_TASK_RUN):
            self.assertTrue(self.ssh.ran(command), command)

    def test_mkdir_command_is_valid_powershell(self):
        # Regression test: the command used to be a plain string with
        # doubled braces and a literal "{tmp}".
        _win().install_chrome('120')
        self.assertTrue(self.ssh.ran(_WIN_MKDIR))
        for command in self.ssh.remote_commands:
            self.assertNotIn('{{', command)
            self.assertNotIn('{tmp}', command)

    def test_mkdir_failure_raises(self):
        self.ssh.responses[_WIN_MKDIR] = completed(1, stderr='access denied')
        with self.assertRaises(RuntimeError) as ctx:
            _win().install_chrome('120')
        self.assertIn('creating C:/cft_temp', str(ctx.exception))
        self.assertFalse(self.ssh.ran_prefix(_WIN_SETUP_PREFIX))

    def test_setup_failure_raises(self):
        self.ssh.default = completed(1, stderr='bad zip')
        self.ssh.responses[_WIN_MKDIR] = completed()
        with self.assertRaises(RuntimeError) as ctx:
            _win().install_chrome('120')
        self.assertIn('bad zip', str(ctx.exception))
        self.assertFalse(self.ssh.ran(_WIN_TASK_CREATE))

    def test_missing_after_setup_raises_even_if_rc_0(self):
        # curl.exe/tar.exe failures do not set PowerShell's exit code.
        self.ssh.responses[_WIN_INSTALLED_CHECK] = _MISSING
        with self.assertRaises(RuntimeError):
            _win().install_chrome('120')
        self.assertFalse(self.ssh.ran(_WIN_TASK_CREATE))

    def test_schtasks_create_failure_raises(self):
        self.ssh.responses[_WIN_TASK_CREATE] = completed(
            1, stderr='ERROR: Access is denied.'
        )
        with self.assertRaises(RuntimeError) as ctx:
            _win().install_chrome('120')
        self.assertIn('scheduling chromedriver', str(ctx.exception))
        self.assertIn('Access is denied', str(ctx.exception))
        self.assertFalse(self.ssh.ran(_WIN_TASK_RUN))

    def test_schtasks_delete_failure_is_tolerated(self):
        self.ssh.responses[_WIN_TASK_DELETE] = completed(
            1, stderr='ERROR: The system cannot find the file specified.'
        )
        _win().install_chrome('120')
        self.assertTrue(self.ssh.ran(_WIN_TASK_RUN))


class ChromeBinaryPathTest(unittest.TestCase):
    def test_per_os(self):
        cases = {
            'mac': (
                '/tmp/c/Google Chrome for Testing.app',
                '/tmp/c/Google Chrome for Testing.app/Contents/MacOS/'
                'Google Chrome for Testing',
            ),
            'linux': ('/tmp/c/chrome', '/tmp/c/chrome'),
            'win': (_WIN_APP, _WIN_APP),
        }
        for sender_os, (app_path, binary) in cases.items():
            with self.subTest(sender_os=sender_os):
                sender = senders.make_sender(make_args(sender_os=sender_os))
                self.assertEqual(sender.chrome_binary_path(app_path), binary)


class LocalInstallTest(fakes.SenderTestCase):
    def test_app_path_follows_sender_os(self):
        self.run.side_effect = None
        self.run.return_value = completed()
        self._patch('os.path.exists', return_value=True)
        cases = {
            'mac': '/tmp/chrome-mac-arm64/Google Chrome for Testing.app',
            'linux': '/tmp/chrome-mac-arm64/chrome',
            'win': '/tmp/chrome-mac-arm64/chrome.exe',
        }
        for sender_os, expected in cases.items():
            with self.subTest(sender_os=sender_os):
                sender = senders.make_sender(
                    make_args(sender='localhost', sender_os=sender_os)
                )
                with self.assertLogs(level='INFO'):
                    # pylint: disable-next=protected-access
                    app_path = sender._install_locally(_CHROME_URL, _DRIVER_URL)
                self.assertEqual(app_path, expected)
                self.assertEqual(sender.driver_path, _MAC_DRIVER)


class CleanupTest(fakes.SenderTestCase):
    def test_cleanup_binaries_skips_when_sender_unreachable(self):
        self.fail_preflight()
        with self.assertLogs(level='INFO') as logs:
            _mac().cleanup_binaries()
        self.run.assert_not_called()
        self.assertIn(fakes.SKIP_LOG, logs.output)
        self.assertFalse(
            any('Cleaned up remote' in l for l in logs.output), logs.output
        )
        self.assertFalse(any('confirmed gone' in l for l in logs.output))

    def test_cleanup_binaries_skips_when_terminate_finds_255(self):
        self.ssh.responses[_MAC_CHECK] = completed(255, stderr=NO_ROUTE)
        with self.assertLogs(level='INFO') as logs:
            _mac().cleanup_binaries()
        self.assertFalse(self.ssh.ran(_MAC_RM_BINARIES))
        self.assertIn(fakes.SKIP_LOG, logs.output)

    def test_cleanup_binaries_warns_on_nonzero_rc(self):
        self.ssh.responses[_MAC_RM_BINARIES] = completed(
            1, stderr='rm: permission denied'
        )
        with self.assertLogs(level='INFO') as logs:
            _mac().cleanup_binaries()
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
            _mac().cleanup_binaries()
        self.assertIn(
            'INFO:root:Cleaned up remote Chrome/Chromedriver directories.',
            logs.output,
        )

    def test_cleanup_binaries_never_raises(self):
        _mac().verify_connectivity()
        self.run.side_effect = TimeoutError('ssh hung')
        with self.assertLogs(level='WARNING'):
            _mac().cleanup_binaries()

    def test_cleanup_commands_per_os(self):
        expected = {
            'mac': ('rm -f /tmp/*.zip', _MAC_RM_BINARIES),
            'linux': ('rm -f /tmp/*.zip', _MAC_RM_BINARIES),
            'cros': (
                'rm -f /usr/local/tmp/*.zip',
                'rm -rf /usr/local/tmp/chrome* /usr/local/tmp/chromedriver*',
            ),
            'win': (
                'powershell -Command "Remove-Item -Path C:/cft_temp/*.zip '
                '-Force -ErrorAction SilentlyContinue"',
                'powershell -Command "Remove-Item -Path '
                'C:/cft_temp/chrome*,C:/cft_temp/chromedriver* '
                '-Recurse -Force -ErrorAction SilentlyContinue"',
            ),
        }
        for sender_os, (downloads, binaries) in expected.items():
            with self.subTest(sender_os=sender_os):
                sender = senders.make_sender(make_args(sender_os=sender_os))
                # pylint: disable=protected-access
                self.assertEqual(sender._downloads_cleanup_command(), downloads)
                self.assertEqual(sender._binaries_cleanup_command(), binaries)
                # pylint: enable=protected-access


class MonitoringTest(fakes.SenderTestCase):
    def test_glances_start_and_stop(self):
        sender = _mac()
        sender.start_monitoring('/tmp/g.csv')
        spawned = self.popen.call_args[0][0][-1]
        self.assertEqual(
            spawned,
            'python3 -m glances -t 1 --export csv '
            '--export-csv-file /tmp/g.csv --quiet',
        )

        proc = mock.MagicMock()
        sender.stop_monitoring(proc, '/tmp/g.csv', '/out/g.csv')
        proc.terminate.assert_called_once()
        self.assertEqual(
            self.ssh.remote_commands, ['pkill -f glances', 'rm -f /tmp/g.csv']
        )
        self.assertEqual(
            self.ssh.copies[0][-2:],
            [f'{fakes.USER}@{MAC_HOST}:/tmp/g.csv', '/out/g.csv'],
        )

    def test_cros_uses_power_log(self):
        sender = _mac(sender_os='cros')
        sender.start_monitoring('/tmp/g.csv')
        self.assertIn('power_now', self.popen.call_args[0][0][-1])

        sender.stop_monitoring(None, '/tmp/g.csv', '/out/g.csv')
        self.assertIn('rm -f /tmp/cros_power.txt', self.ssh.remote_commands)
        self.assertEqual(self.ssh.copies[0][-1], '/out/g.csv')

    def test_windows_removes_csv_with_powershell(self):
        _win().stop_monitoring(None, 'C:/cft_temp/g.csv', '/out/g.csv')
        self.assertEqual(
            self.ssh.remote_commands[-1],
            "powershell -Command \"Remove-Item -Path 'C:/cft_temp/g.csv' "
            "-Force -ErrorAction SilentlyContinue\"",
        )
        self.assertFalse(self.ssh.ran_prefix('rm '))

    def test_kills_monitor_that_ignores_terminate(self):
        proc = mock.MagicMock()
        proc.wait.side_effect = subprocess.TimeoutExpired('ssh', 5)
        with self.assertLogs(level='WARNING'):
            _mac().stop_monitoring(proc, '/tmp/g.csv', '/out/g.csv')
        proc.terminate.assert_called_once()
        proc.kill.assert_called_once()


if __name__ == '__main__':
    unittest.main()
