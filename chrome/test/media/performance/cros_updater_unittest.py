#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for ChromeOS Canary auto-updating in cros_updater.py."""

import subprocess
import unittest
from unittest import mock

import perf_test_fakes as fakes
from perf_test_fakes import completed, make_args
import common
import cros_updater
import openscreen_cast_performance_test as cast_test

_CROS_HOST = 'lin-152-e504--cros'
_LSB_TEMPLATE = (
    'CHROMEOS_RELEASE_BOARD={board}\nCHROMEOS_RELEASE_VERSION={version}\n'
)


def _cros_args():
    return make_args(sender=_CROS_HOST, username='root', sender_os='cros')


def _fake_popen_pair():
    """Returns a Popen side_effect supporting gsutil -> ssh streaming pipes."""

    def _popen(argv, **kwargs):
        del kwargs
        proc = mock.MagicMock()
        proc.__enter__ = mock.Mock(return_value=proc)
        proc.__exit__ = mock.Mock(return_value=False)
        proc.returncode = 0
        proc.wait.return_value = 0
        if argv[0] == 'ssh':
            proc.communicate.return_value = (b'', b'')
        else:
            proc.stdout = mock.MagicMock()
            proc.stderr = mock.MagicMock()
            proc.stderr.read.return_value = b''
        return proc

    return _popen


class ParseHelpersTest(unittest.TestCase):
    def test_parse_lsb_release_strips_signed_suffix(self):
        board, version = cros_updater.parse_lsb_release(
            _LSB_TEMPLATE.format(
                board='dedede-signed-mp-v3keys', version='16521.0.0'
            )
        )
        self.assertEqual(board, 'dedede')
        self.assertEqual(version, '16521.0.0')

    def test_parse_lsb_release_rejects_missing_fields(self):
        with self.assertRaises(RuntimeError):
            cros_updater.parse_lsb_release('CHROMEOS_RELEASE_BOARD=dedede\n')

    def test_parse_latest_build_valid_and_invalid(self):
        self.assertEqual(
            cros_updater.parse_latest_build('R160-16886.0.0\n'),
            ('R160-16886.0.0', '16886.0.0'),
        )
        with self.assertRaises(RuntimeError):
            cros_updater.parse_latest_build('16886.0.0')


class EnsureLatestCanaryTest(fakes.SenderTestCase):
    def setUp(self):
        super().setUp()
        self.latest_build = 'R160-16886.0.0\n'
        self.gsutil_calls = []

        def _dispatch(argv, **kwargs):
            if argv[0] in ('ssh', 'scp'):
                return self.ssh(argv, **kwargs)
            self.gsutil_calls.append(argv)
            return subprocess.CompletedProcess(
                args=argv, returncode=0, stdout=self.latest_build, stderr=''
            )

        self.run.side_effect = _dispatch
        self.popen.side_effect = _fake_popen_pair()
        self._patch(
            'cros_updater._find_gsutil_cmd', return_value=['/tmp/gsutil']
        )
        self._patch(
            'cros_updater._read_host_public_key',
            return_value='ssh-ed25519 AAAAC3NzaC1lZDI1NTE5 chrome-bot@host',
        )
        self._patch(
            'remote_transport.get_testing_rsa_path',
            return_value='/tmp/fake_testing_rsa',
        )

    def test_skips_flash_when_already_on_latest_canary(self):
        self.ssh.responses['cat /etc/lsb-release'] = completed(
            stdout=_LSB_TEMPLATE.format(board='dedede', version='16886.0.0')
        )

        flashed = cros_updater.ensure_latest_canary(_cros_args())

        self.assertFalse(flashed)
        self.assertEqual(
            self.gsutil_calls,
            [
                [
                    '/tmp/gsutil',
                    'cat',
                    'gs://chromeos-image-archive/dedede-release/LATEST-main',
                ]
            ],
        )
        self.popen.assert_not_called()

    def test_flashes_slot_b_when_behind(self):
        self.ssh.responses['cat /etc/lsb-release'] = [
            completed(
                stdout=_LSB_TEMPLATE.format(board='dedede', version='16521.0.0')
            ),
            completed(
                stdout=_LSB_TEMPLATE.format(board='dedede', version='16886.0.0')
            ),
        ]
        self.ssh.responses['rootdev -s'] = completed(stdout='/dev/mmcblk0p3\n')

        flashed = cros_updater.ensure_latest_canary(_cros_args())

        self.assertTrue(flashed)
        streamed_urls = [
            call.args[0][-1]
            for call in self.popen.call_args_list
            if call.args[0][0] == '/tmp/gsutil'
        ]
        self.assertEqual(
            streamed_urls,
            [
                'gs://chromeos-image-archive/dedede-release/R160-16886.0.0/'
                'full_dev_part_KERN.bin.gz',
                'gs://chromeos-image-archive/dedede-release/R160-16886.0.0/'
                'full_dev_part_ROOT.bin.gz',
                'gs://chromeos-image-archive/dedede-release/R160-16886.0.0/'
                'stateful.tgz',
            ],
        )
        for call in self.popen.call_args_list:
            if call.args[0][0] == '/tmp/gsutil':
                self.assertEqual(call.args[0][:3], ['/tmp/gsutil', '-q', 'cat'])
                self.assertNotEqual(call.kwargs['stderr'], subprocess.PIPE)
        self.assertTrue(
            any(
                'postinst" /dev/mmcblk0p5' in cmd
                for cmd in self.ssh.remote_commands
            )
        )
        self.assertTrue(
            any('reboot' in cmd for cmd in self.ssh.remote_commands)
        )
        self.assertTrue(
            any(
                '/root/.ssh/authorized_keys' in cmd
                and 'ssh-ed25519 AAAAC3NzaC1lZDI1NTE5' in cmd
                for cmd in self.ssh.remote_commands
            )
        )

    def test_raises_if_post_flash_version_mismatch(self):
        self.ssh.responses['cat /etc/lsb-release'] = [
            completed(
                stdout=_LSB_TEMPLATE.format(board='dedede', version='16521.0.0')
            ),
            completed(
                stdout=_LSB_TEMPLATE.format(board='dedede', version='16521.0.0')
            ),
        ]
        self.ssh.responses['rootdev -s'] = completed(stdout='/dev/sda3\n')

        with self.assertRaises(RuntimeError):
            cros_updater.ensure_latest_canary(_cros_args())

    def test_unreachable_dut_stops_before_gcs_lookup(self):
        self.make_unreachable()
        with self.assertLogs(level='WARNING'):
            with self.assertRaises(common.SenderUnreachableError):
                cros_updater.ensure_latest_canary(_cros_args())
        self.assertEqual(self.gsutil_calls, [])
        self.popen.assert_not_called()

    def test_falls_back_to_testing_rsa_on_permission_denied(self):
        self.ssh.responses['cat /etc/lsb-release'] = completed(
            stdout=_LSB_TEMPLATE.format(board='dedede', version='16886.0.0')
        )
        orig_dispatch = self.run.side_effect
        saw_testing_rsa = []

        def dispatch_with_fallback(argv, **kwargs):
            if (
                argv[0] == 'ssh'
                and argv[-1] == 'echo ok'
                and 'PubkeyAcceptedAlgorithms=+ssh-rsa' in argv
            ):
                saw_testing_rsa.append(argv)
                return completed(stdout='ok\n')
            if argv[0] == 'ssh' and argv[-1] == 'echo ok':
                return completed(returncode=255, stderr='Permission denied')
            return orig_dispatch(argv, **kwargs)

        self.run.side_effect = dispatch_with_fallback
        with self.assertLogs(level='WARNING'):
            flashed = cros_updater.ensure_latest_canary(_cros_args())

        self.assertFalse(flashed)
        self.assertTrue(saw_testing_rsa)
        self.assertTrue(
            any(
                '/root/.ssh/authorized_keys' in cmd
                for cmd in self.ssh.remote_commands
            )
        )

    def test_postinst_runs_before_stateful_so_failure_skips_stateful(self):
        self.ssh.responses['cat /etc/lsb-release'] = completed(
            stdout=_LSB_TEMPLATE.format(board='dedede', version='16521.0.0')
        )
        self.ssh.responses['rootdev -s'] = completed(stdout='/dev/mmcblk0p3\n')
        orig_dispatch = self.run.side_effect

        def fail_postinst(argv, **kwargs):
            if argv[0] == 'ssh' and 'postinst' in argv[-1]:
                return completed(returncode=1, stderr='postinst failed')
            return orig_dispatch(argv, **kwargs)

        self.run.side_effect = fail_postinst
        with self.assertRaisesRegex(RuntimeError, 'postinst failed'):
            cros_updater.ensure_latest_canary(_cros_args())

        streamed_urls = [
            call.args[0][-1]
            for call in self.popen.call_args_list
            if call.args[0][0] == '/tmp/gsutil'
        ]
        self.assertEqual(
            streamed_urls,
            [
                'gs://chromeos-image-archive/dedede-release/R160-16886.0.0/'
                'full_dev_part_KERN.bin.gz',
                'gs://chromeos-image-archive/dedede-release/R160-16886.0.0/'
                'full_dev_part_ROOT.bin.gz',
            ],
        )

    def test_stateful_failure_cleans_up_update_available_and_reverts_slot(self):
        self.ssh.responses['cat /etc/lsb-release'] = completed(
            stdout=_LSB_TEMPLATE.format(board='dedede', version='16521.0.0')
        )
        self.ssh.responses['rootdev -s'] = completed(stdout='/dev/mmcblk0p3\n')
        base_popen = _fake_popen_pair()

        def fail_stateful(argv, **kwargs):
            proc = base_popen(argv, **kwargs)
            if argv[0] == 'ssh' and 'stateful_partition' in argv[-1]:
                proc.returncode = 1
                proc.communicate.return_value = (b'', b'tar failed')
            return proc

        self.popen.side_effect = fail_stateful
        with self.assertRaisesRegex(RuntimeError, 'tar failed'):
            cros_updater.ensure_latest_canary(_cros_args())

        self.assertTrue(
            any(
                'rm -rf /mnt/stateful_partition/.update_available' in cmd
                for cmd in self.ssh.remote_commands
            )
        )
        self.assertIn('/postinst /dev/mmcblk0p3', self.ssh.remote_commands)

    def test_stream_kills_both_processes_on_timeout(self):
        self.ssh.responses['cat /etc/lsb-release'] = completed(
            stdout=_LSB_TEMPLATE.format(board='dedede', version='16521.0.0')
        )
        self.ssh.responses['rootdev -s'] = completed(stdout='/dev/mmcblk0p3\n')
        procs = {}

        def timeout_popen(argv, **kwargs):
            del kwargs
            proc = mock.MagicMock()
            proc.__enter__ = mock.Mock(return_value=proc)
            proc.__exit__ = mock.Mock(return_value=False)
            if argv[0] == 'ssh':
                proc.communicate.side_effect = subprocess.TimeoutExpired(
                    argv, 600
                )
                procs['ssh'] = proc
            else:
                proc.stdout = mock.MagicMock()
                procs['gs'] = proc
            return proc

        self.popen.side_effect = timeout_popen
        with self.assertRaises(subprocess.TimeoutExpired):
            cros_updater.ensure_latest_canary(_cros_args())

        procs['ssh'].kill.assert_called_once()
        procs['ssh'].wait.assert_called_once()
        procs['gs'].kill.assert_called_once()
        procs['gs'].wait.assert_called_once()

    def test_stream_reports_gsutil_stderr_from_temp_file_on_failure(self):
        self.ssh.responses['cat /etc/lsb-release'] = completed(
            stdout=_LSB_TEMPLATE.format(board='dedede', version='16521.0.0')
        )
        self.ssh.responses['rootdev -s'] = completed(stdout='/dev/mmcblk0p3\n')
        base_popen = _fake_popen_pair()

        def fail_gsutil(argv, **kwargs):
            proc = base_popen(argv, **kwargs)
            if argv[0] == '/tmp/gsutil':
                stderr_file = kwargs['stderr']
                self.assertNotEqual(stderr_file, subprocess.PIPE)
                stderr_file.write(b'AccessDeniedException: 403\n')
                stderr_file.flush()
                proc.wait.return_value = 1
            return proc

        self.popen.side_effect = fail_gsutil
        with self.assertRaisesRegex(RuntimeError, 'AccessDeniedException: 403'):
            cros_updater.ensure_latest_canary(_cros_args())


class CastPerfEntryPointTest(fakes.SenderTestCase):
    def _run_main(self, argv):
        self._patch('sys.argv', new=argv)
        self._patch('subprocess.run', return_value=completed())
        self._patch('common.verify_receiver_connectivity')
        self._patch('test_results.result_sink.TryInitClient', return_value=None)
        self._patch(
            'openscreen_cast_performance_test.setup_test_environment',
            return_value=cast_test.SenderSession(
                driver=mock.MagicMock(),
                tunnel_proc=None,
                chrome_version='160.0.0.0',
                trace_file_path=None,
            ),
        )
        self._patch('openscreen_cast_performance_test.run_performance_test')
        self._patch('common.teardown_test_environment')
        self._patch('common.cleanup_binaries')
        self._patch('common.finalize_results')
        self._patch('shutil.rmtree')
        self._patch('os.makedirs')
        return cast_test.main()

    def test_cros_calls_updater_unless_skipped(self):
        with mock.patch('cros_updater.ensure_latest_canary') as update:
            self._run_main(
                ['prog', '--sender', _CROS_HOST, '--sender-os', 'cros']
            )
            update.assert_called_once()

            update.reset_mock()
            self._run_main(
                [
                    'prog',
                    '--sender',
                    _CROS_HOST,
                    '--sender-os',
                    'cros',
                    '--skip-cros-update',
                ]
            )
            update.assert_not_called()

    def test_non_cros_does_not_call_updater(self):
        with mock.patch('cros_updater.ensure_latest_canary') as update:
            self._run_main(
                ['prog', '--sender', fakes.MAC_HOST, '--sender-os', 'mac']
            )
            update.assert_not_called()


if __name__ == '__main__':
    unittest.main()
