#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for the common.py wrappers used by the perf test scripts."""

import subprocess
import unittest
from unittest import mock

from selenium.common.exceptions import WebDriverException
import urllib3

import perf_test_fakes as fakes
from perf_test_fakes import NO_ROUTE, completed, make_args
import common
import cros_setup
import media_metrics


class ReexportTest(unittest.TestCase):
    def test_scripts_can_still_reach_shared_names(self):
        for name in (
            'REPO_ROOT',
            'TRACES_DIR',
            'RECORDINGS_DIR',
            'SERVER_PORT',
            'REMOTE_URL',
            'LOCAL_HOST_IP',
            'WIN_REMOTE_TMP_DIR',
            'FAIL_CODE',
            'METRICS',
            'VIDEOS',
            'measures',
            'server',
            'video_analyzer',
            'StartProcess',
            'make_sender',
            'setup_cros_environment',
            'calculate_psnr_ssim',
            'finalize_results',
            'parse_glances_csv_and_record',
            'RemoteDeviceError',
        ):
            with self.subTest(name=name):
                self.assertTrue(hasattr(common, name))


class AcceptanceTest(fakes.SenderTestCase):
    """An unreachable Mac sender stops setup before uname or CfT download."""

    def test_unreachable_mac_fails_fast_with_actionable_error(self):
        self.make_unreachable()
        sender = common.make_sender(make_args())
        with mock.patch('senders.download_cft_urls') as download:
            with self.assertLogs(level='INFO') as logs:
                with self.assertRaises(common.SenderUnreachableError) as ctx:
                    sender.verify_connectivity()
                    sender.terminate_chromedriver()
                    sender.install_chrome(None)
                common.teardown_test_environment(None, None, make_args())
                common.cleanup_binaries(make_args())
        download.assert_not_called()
        self.run.assert_not_called()

        msg = str(ctx.exception)
        self.assertIn('is not reachable on port 22', msg)
        self.assertIn('infra failure, not a test failure', msg)
        self.assertNotIn('Unsupported OS/Arch', msg)
        for line in logs.output:
            self.assertNotIn('confirmed gone', line)
            self.assertNotIn('Cleaned up remote', line)
            self.assertNotIn('Cleaned up tmp files', line)

    def test_setup_cros_environment_stops_before_crossbench(self):
        self.make_unreachable()
        with (
            mock.patch('senders.download_cft_urls') as download,
            mock.patch('cros_setup._prepare_crossbench_imports') as prepare,
        ):
            with self.assertLogs(level='WARNING'):
                with self.assertRaises(common.SenderUnreachableError):
                    common.setup_cros_environment(
                        make_args(sender_os='cros'), None, []
                    )
            download.assert_not_called()
            prepare.assert_not_called()
        self.run.assert_not_called()


class TeardownTest(fakes.SenderTestCase):
    def test_teardown_skips_remote_cleanup_when_unreachable(self):
        self.fail_preflight()
        with self.assertLogs(level='WARNING') as logs:
            common.teardown_test_environment(None, None, make_args())
        self.run.assert_not_called()
        self.assertIn(fakes.SKIP_LOG, logs.output)

    def test_teardown_warns_on_nonzero_rc(self):
        self.ssh.responses['rm -f /tmp/*.zip'] = completed(255, stderr=NO_ROUTE)
        with self.assertLogs(level='INFO') as logs:
            common.teardown_test_environment(None, None, make_args())
        self.assertFalse(
            any('Cleaned up tmp files' in l for l in logs.output),  # noqa: E741
            logs.output,  # noqa: E741
        )
        self.assertTrue(any('rc=255' in l for l in logs.output))  # noqa: E741

    def test_teardown_logs_success(self):
        with self.assertLogs(level='INFO') as logs:
            common.teardown_test_environment(None, None, make_args())
        self.assertIn(
            'INFO:root:Cleaned up tmp files on remote machine.', logs.output
        )

    def test_teardown_never_raises(self):
        self.run.side_effect = subprocess.TimeoutExpired(cmd='ssh', timeout=120)
        with self.assertLogs(level='WARNING'):
            common.teardown_test_environment(None, None, make_args())

    def test_teardown_stops_driver_and_tunnel(self):
        driver = mock.MagicMock()
        tunnel = mock.MagicMock()
        tunnel.poll.return_value = None
        with self.assertLogs(level='INFO'):
            common.teardown_test_environment(driver, tunnel, make_args())
        driver.quit.assert_called_once()
        tunnel.terminate.assert_called_once()

    def test_teardown_stops_crossbench_forwarding(self):
        cb_platform = mock.MagicMock(spec=['ports'])
        with self.assertLogs(level='INFO'):
            common.teardown_test_environment(
                None, cb_platform, make_args(sender_os='cros')
            )
        cb_platform.ports.stop_reverse_forward.assert_called_once_with(
            common.SERVER_PORT
        )

    def test_unknown_sender_os_does_not_raise(self):
        with self.assertLogs(level='WARNING'):
            common.teardown_test_environment(
                None, None, make_args(sender_os=None)
            )
            common.cleanup_binaries(make_args(sender_os=None))
        self.run.assert_not_called()


class CleanupBinariesTest(fakes.SenderTestCase):
    def test_cleanup_binaries_skips_when_sender_unreachable(self):
        self.fail_preflight()
        with self.assertLogs(level='INFO') as logs:
            common.cleanup_binaries(make_args(), '120.0.1.2')
        self.run.assert_not_called()
        self.assertIn(fakes.SKIP_LOG, logs.output)

    def test_cleanup_binaries_logs_success_on_rc_0(self):
        with self.assertLogs(level='INFO') as logs:
            common.cleanup_binaries(make_args())
        self.assertIn(
            'INFO:root:Cleaned up remote Chrome/Chromedriver directories.',
            logs.output,
        )


class DumpRemoteLogsTest(fakes.SenderTestCase):
    def test_uses_per_os_log_command(self):
        cases = {
            'mac': 'cat /tmp/chromedriver_console*.log 2>/dev/null || true',
            'cros': 'cat /usr/local/tmp/chromedriver_console*.log 2>/dev/null || true',
            'win': 'powershell -Command "Get-Content -Path '
            'C:/cft_temp/chromedriver-win*/chromedriver_console*.log '
            '-ErrorAction SilentlyContinue"',
        }
        for sender_os, command in cases.items():
            with self.subTest(sender_os=sender_os):
                self.ssh.responses[command] = completed(stdout='log line')
                with self.assertLogs(level='ERROR') as logs:
                    common.dump_remote_logs(
                        make_args(sender_os=sender_os), '120', 'vp8'
                    )
                self.assertTrue(any('log line' in l for l in logs.output))  # noqa: E741


class ConnectToRemoteDriverTest(unittest.TestCase):
    def setUp(self):
        super().setUp()
        patcher = mock.patch('time.sleep')
        self.addCleanup(patcher.stop)
        patcher.start()

    def test_retries_while_tunnel_is_down(self):
        tunnel_down = urllib3.exceptions.MaxRetryError(None, common.REMOTE_URL)
        driver = mock.MagicMock()
        with mock.patch(
            'selenium.webdriver.Remote', side_effect=[tunnel_down, driver]
        ) as remote:
            with self.assertLogs(level='INFO'):
                self.assertIs(common.connect_to_remote_driver('opts'), driver)
        self.assertEqual(remote.call_count, 2)

    def test_gives_up_with_last_error(self):
        error = WebDriverException('session not created')
        with mock.patch('selenium.webdriver.Remote', side_effect=error):
            with self.assertLogs(level='INFO'):
                with self.assertRaises(RuntimeError) as ctx:
                    common.connect_to_remote_driver('opts')
        self.assertIs(ctx.exception.__cause__, error)

    def test_does_not_retry_unexpected_errors(self):
        with mock.patch(
            'selenium.webdriver.Remote', side_effect=TypeError('bug')
        ) as remote:
            with self.assertLogs(level='INFO'):
                with self.assertRaises(TypeError):
                    common.connect_to_remote_driver('opts')
        remote.assert_called_once()


class TeardownDriverTest(fakes.SenderTestCase):
    def test_dead_session_still_stops_tunnel(self):
        driver = mock.MagicMock()
        driver.quit.side_effect = urllib3.exceptions.MaxRetryError(None, 'u')
        tunnel = mock.MagicMock()
        tunnel.poll.return_value = None
        with self.assertLogs(level='WARNING'):
            common.teardown_test_environment(driver, tunnel, make_args())
        tunnel.terminate.assert_called_once()


class CrosSetupTest(unittest.TestCase):
    def test_install_chromedriver_returns_sender_driver_path(self):
        sender = mock.MagicMock(driver_path='/usr/local/tmp/cd/chromedriver')
        path = cros_setup.install_chromedriver(sender, '120')
        sender.install_chrome.assert_called_once_with('120')
        self.assertEqual(path, '/usr/local/tmp/cd/chromedriver')

    def test_install_chromedriver_failure_propagates(self):
        sender = mock.MagicMock()
        sender.install_chrome.side_effect = RuntimeError('curl failed')
        with self.assertRaisesRegex(RuntimeError, 'curl failed'):
            cros_setup.install_chromedriver(sender, '120')

    def test_detect_milestone(self):
        cb_platform = mock.MagicMock()
        cb_platform.app_version.return_value = 'Google Chrome 129.0.6668.100 '
        with self.assertLogs(level='INFO'):
            self.assertEqual(cros_setup.detect_milestone(cb_platform), '129')

    def test_unparsable_version_raises_instead_of_using_latest(self):
        cb_platform = mock.MagicMock()
        cb_platform.app_version.return_value = 'Google Chrome dev'
        with self.assertRaisesRegex(RuntimeError, 'Google Chrome dev'):
            cros_setup.detect_milestone(cb_platform)


class MediaMetricsTest(unittest.TestCase):
    def test_result_upload_failure_propagates(self):
        client = mock.MagicMock()
        client.UpdateInvocationExtendedProperties.side_effect = OSError('503')
        with (
            mock.patch('media_metrics.measures'),
            mock.patch(
                'media_metrics.result_sink.TryInitClient', return_value=client
            ),
        ):
            with self.assertLogs(level='INFO'):
                with self.assertRaisesRegex(OSError, '503'):
                    media_metrics.finalize_results('120.0.1.2')

    def test_ffmpeg_timeout_is_logged(self):
        with mock.patch(
            'subprocess.run',
            side_effect=subprocess.TimeoutExpired('ffmpeg', 120),
        ):
            with self.assertLogs(level='ERROR') as logs:
                media_metrics.calculate_psnr_ssim('v', 'rec.mp4', 'orig.mp4')
        self.assertEqual(len(logs.output), 2)

    def test_bad_monitoring_log_is_logged(self):
        with (
            mock.patch('os.path.exists', return_value=True),
            mock.patch(
                'builtins.open', mock.mock_open(read_data='12\nnot-a-number\n')
            ),
        ):
            with self.assertLogs(level='ERROR'):
                media_metrics.parse_glances_csv_and_record(
                    'v', '/out/p.txt', 'cros'
                )

    def test_monitoring_parser_bugs_are_not_swallowed(self):
        with (
            mock.patch('os.path.exists', return_value=True),
            mock.patch(
                'media_metrics._record_glances_csv',
                side_effect=TypeError('bug'),
            ),
        ):
            with self.assertRaises(TypeError):
                media_metrics.parse_glances_csv_and_record(
                    'v', '/out/g.csv', 'mac'
                )


if __name__ == '__main__':
    unittest.main()
