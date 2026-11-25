#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for openscreen_cast_performance_test.py."""

import json
import os
import re
import tempfile
import unittest
from unittest import mock

import common
import openscreen_cast_performance_test as cast_test
from perf_test_fakes import completed, make_args
import test_results


class TraceConfigTest(unittest.TestCase):
    def test_receiver_perfetto_config_enables_cast_categories(self):
        cfg = cast_test.PERFETTO_CONFIG
        self.assertIn('name: "android.track_event"', cfg)
        self.assertIn('name: "track_event"', cfg)
        self.assertIn('name: "org.chromium.trace_event"', cfg)
        # Chromecast Android 12 /system/bin/perfetto --txt rejects the newer
        # track_event_config proto field on DataSourceConfig.
        self.assertNotIn('track_event_config', cfg)
        for cat in ('openscreen', 'cast.stream', 'media'):
            self.assertIn(rf'\"{cat}\"', cfg)

    def test_receiver_perfetto_config_uses_double_quoted_strings(self):
        # Perfetto's `--txt` parser (`text_to_proto.cc`) only accepts `"` for
        # string values and rejects single-quoted strings.
        single_quoted = re.findall(r':\s*\'', cast_test.PERFETTO_CONFIG)
        self.assertEqual(single_quoted, [])

    def test_sender_trace_categories_use_real_cast_names(self):
        cats = set(cast_test.SENDER_TRACE_CATEGORIES.split(','))
        self.assertTrue(
            {'openscreen', 'cast.stream', 'cast_perf_test', 'media'} <= cats
        )
        self.assertNotIn('cast', cats)


class MainLoopContinuationTest(unittest.TestCase):
    def test_later_videos_still_run_and_collect_traces_after_clip_failure(self):
        ran_videos = []
        copied_traces = []

        def fake_setup(_args, _cv, _opts, codec_name, video_name):
            trace = f'/remote/{codec_name}_{video_name}_sender.perfetto-trace'
            return cast_test.SenderSession(
                mock.MagicMock(), mock.MagicMock(), '159.0.0.1', trace
            )

        def fake_run(video_name, _driver, codec_name, _args):
            key = f'{codec_name}_{video_name}'
            ran_videos.append(key)
            if key == 'H264_1080p30fpsH264_foodmarket_yt_sync.mp4':
                raise RuntimeError('simulated video analyzer failure')
            return mock.MagicMock()

        fake_sender = mock.MagicMock()
        fake_sender.codec_skip_reason.return_value = None
        fake_sender.copy_from.side_effect = lambda src, dst, timeout=60: (
            copied_traces.append((src, dst))
        )

        with tempfile.TemporaryDirectory() as tmp_dir:
            results_path = os.path.join(tmp_dir, 'output.json')
            reporter = test_results.TestReporter(output_path=results_path)
            with (
                mock.patch.object(common, 'RECORDINGS_DIR', f'{tmp_dir}/rec'),
                mock.patch.object(common, 'TRACES_DIR', f'{tmp_dir}/traces'),
                mock.patch.object(common, 'verify_receiver_connectivity'),
                mock.patch.object(
                    common, 'make_sender', return_value=fake_sender
                ),
                mock.patch.object(common, 'dump_remote_logs'),
                mock.patch.object(common, 'teardown_recording_process'),
                mock.patch.object(common, 'teardown_test_environment'),
                mock.patch.object(common, 'cleanup_binaries'),
                mock.patch.object(common, 'finalize_results') as finalize,
                mock.patch.object(
                    cast_test, 'should_run_vp9_tests', return_value=False
                ),
                mock.patch.object(
                    cast_test, 'should_run_60fps_vp8_tests', return_value=True
                ),
                mock.patch.object(
                    cast_test, 'setup_test_environment', side_effect=fake_setup
                ),
                mock.patch.object(
                    cast_test, 'run_performance_test', side_effect=fake_run
                ),
                mock.patch.object(
                    cast_test.subprocess, 'run', return_value=completed()
                ),
                mock.patch.object(cast_test.time, 'sleep'),
                mock.patch.object(
                    cast_test.sys,
                    'argv',
                    [
                        'openscreen_cast_performance_test.py',
                        '--sender=127.0.0.1',
                        '--sender-os=mac',
                        '--receiver=sink',
                    ],
                ),
            ):
                with self.assertRaisesRegex(
                    RuntimeError, 'simulated video analyzer failure'
                ):
                    with reporter:
                        cast_test._run_tests(reporter)

            with open(results_path, encoding='utf-8') as f:
                results_json = json.load(f)

        # H264 (2 videos) + VP8 (2 videos) all ran despite the first failing.
        self.assertEqual(len(ran_videos), 4)
        self.assertEqual(len(copied_traces), 4)
        finalize.assert_called_once_with('159.0.0.1')
        # Each case keeps its outcome without a duplicate harness error.
        self.assertNotIn(test_results.HARNESS_CASE, results_json['tests'])
        self.assertEqual(
            results_json['tests']['H264'][
                '1080p30fpsH264_foodmarket_yt_sync.mp4'
            ]['actual'],
            'FAIL',
        )
        self.assertEqual(
            results_json['tests']['H264']['1080p60fpsH264_boat_yt_sync.mp4'][
                'actual'
            ],
            'PASS',
        )
        self.assertEqual(
            results_json['tests']['VP8'][
                '1080p30fpsH264_foodmarket_yt_sync.mp4'
            ]['actual'],
            'PASS',
        )
        self.assertEqual(
            results_json['tests']['VP8']['1080p60fpsH264_boat_yt_sync.mp4'][
                'actual'
            ],
            'PASS',
        )

    def test_infra_error_aborts_remaining_clips_immediately(self):
        ran_videos = []

        def fake_setup(_args, _cv, _opts, codec_name, video_name):
            trace = f'/remote/{codec_name}_{video_name}_sender.perfetto-trace'
            return cast_test.SenderSession(
                mock.MagicMock(), mock.MagicMock(), '159.0.0.1', trace
            )

        def fake_run(video_name, _driver, codec_name, _args):
            ran_videos.append(f'{codec_name}_{video_name}')
            raise common.SenderUnreachableError('ssh lost')

        fake_sender = mock.MagicMock()
        fake_sender.codec_skip_reason.return_value = None
        with tempfile.TemporaryDirectory() as tmp_dir:
            results_path = os.path.join(tmp_dir, 'output.json')
            reporter = test_results.TestReporter(output_path=results_path)
            with (
                mock.patch.object(common, 'RECORDINGS_DIR', f'{tmp_dir}/rec'),
                mock.patch.object(common, 'TRACES_DIR', f'{tmp_dir}/traces'),
                mock.patch.object(common, 'verify_receiver_connectivity'),
                mock.patch.object(
                    common, 'make_sender', return_value=fake_sender
                ),
                mock.patch.object(common, 'dump_remote_logs'),
                mock.patch.object(common, 'teardown_test_environment'),
                mock.patch.object(common, 'cleanup_binaries'),
                mock.patch.object(common, 'finalize_results'),
                mock.patch.object(
                    cast_test, 'should_run_vp9_tests', return_value=False
                ),
                mock.patch.object(
                    cast_test, 'should_run_60fps_vp8_tests', return_value=True
                ),
                mock.patch.object(
                    cast_test, 'setup_test_environment', side_effect=fake_setup
                ),
                mock.patch.object(
                    cast_test, 'run_performance_test', side_effect=fake_run
                ),
                mock.patch.object(
                    cast_test.subprocess, 'run', return_value=completed()
                ),
                mock.patch.object(cast_test.time, 'sleep'),
                mock.patch.object(
                    cast_test.sys,
                    'argv',
                    [
                        'openscreen_cast_performance_test.py',
                        '--sender=127.0.0.1',
                        '--sender-os=mac',
                        '--receiver=sink',
                    ],
                ),
            ):
                with self.assertRaises(common.SenderUnreachableError):
                    with reporter:
                        cast_test._run_tests(reporter)

        self.assertEqual(len(ran_videos), 1)

    def test_chrome_options_include_common_disabled_features(self):
        seen_options = []

        def fake_setup(_args, _cv, opts, codec_name, video_name):
            seen_options.append((codec_name, list(opts)))
            trace = f'/remote/{codec_name}_{video_name}_sender.perfetto-trace'
            return cast_test.SenderSession(
                mock.MagicMock(), mock.MagicMock(), '159.0.0.1', trace
            )

        fake_sender = mock.MagicMock()
        fake_sender.codec_skip_reason.return_value = None
        with tempfile.TemporaryDirectory() as tmp_dir:
            results_path = os.path.join(tmp_dir, 'output.json')
            reporter = test_results.TestReporter(output_path=results_path)
            with (
                mock.patch.object(common, 'RECORDINGS_DIR', f'{tmp_dir}/rec'),
                mock.patch.object(common, 'TRACES_DIR', f'{tmp_dir}/traces'),
                mock.patch.object(common, 'verify_receiver_connectivity'),
                mock.patch.object(
                    common, 'make_sender', return_value=fake_sender
                ),
                mock.patch.object(common, 'dump_remote_logs'),
                mock.patch.object(common, 'teardown_recording_process'),
                mock.patch.object(common, 'teardown_test_environment'),
                mock.patch.object(common, 'cleanup_binaries'),
                mock.patch.object(common, 'finalize_results'),
                mock.patch.object(
                    cast_test, 'should_run_vp9_tests', return_value=True
                ),
                mock.patch.object(
                    cast_test, 'setup_test_environment', side_effect=fake_setup
                ),
                mock.patch.object(
                    cast_test,
                    'run_performance_test',
                    return_value=mock.MagicMock(),
                ),
                mock.patch.object(
                    cast_test.subprocess, 'run', return_value=completed()
                ),
                mock.patch.object(cast_test.time, 'sleep'),
                mock.patch.object(
                    cast_test.sys,
                    'argv',
                    [
                        'openscreen_cast_performance_test.py',
                        '--sender=127.0.0.1',
                        '--sender-os=win',
                        '--receiver=sink',
                    ],
                ),
            ):
                with reporter:
                    cast_test._run_tests(reporter)

        self.assertEqual(len(seen_options), 6)
        for codec_name, opts in seen_options:
            disable_flags = [
                opt for opt in opts if opt.startswith('--disable-features=')
            ]
            self.assertEqual(len(disable_flags), 1, codec_name)
            disabled = set(
                disable_flags[0].removeprefix('--disable-features=').split(',')
            )
            self.assertTrue(
                set(common.CHROME_DISABLED_FEATURES) <= disabled, codec_name
            )
            self.assertIn('CastStreamingHardwareHevc', disabled)

    def test_setup_failure_dumps_remote_logs_before_cleanup(self):
        call_order = []

        with (
            tempfile.TemporaryDirectory() as tmp_dir,
            mock.patch.object(common, 'RECORDINGS_DIR', f'{tmp_dir}/rec'),
            mock.patch.object(common, 'TRACES_DIR', f'{tmp_dir}/traces'),
            mock.patch.object(common, 'verify_receiver_connectivity'),
            mock.patch.object(
                common,
                'dump_remote_logs',
                side_effect=lambda *_args: call_order.append('dump_logs'),
            ),
            mock.patch.object(
                common,
                'cleanup_binaries',
                side_effect=lambda *_args: call_order.append('cleanup'),
            ),
            mock.patch.object(common, 'finalize_results'),
            mock.patch.object(
                cast_test, 'should_run_vp9_tests', return_value=False
            ),
            mock.patch.object(
                cast_test,
                'setup_test_environment',
                side_effect=RuntimeError('DevToolsActivePort missing'),
            ),
            mock.patch.object(
                cast_test.subprocess, 'run', return_value=completed()
            ),
            mock.patch.object(
                cast_test.sys,
                'argv',
                [
                    'openscreen_cast_performance_test.py',
                    '--sender=127.0.0.1',
                    '--sender-os=win',
                    '--receiver=sink',
                ],
            ),
        ):
            with self.assertLogs(level='ERROR'):
                with self.assertRaisesRegex(
                    RuntimeError, 'DevToolsActivePort missing'
                ):
                    cast_test.main()

        self.assertEqual(call_order, ['dump_logs', 'cleanup'])

    def test_xvfb_startup_error_fails_each_planned_case(self):
        fake_sender = mock.MagicMock()
        fake_sender.codec_skip_reason.side_effect = lambda codec: (
            'The lab host has no hardware H.264 encoder.'
            if codec == 'H264'
            else None
        )
        fake_sender.virtual_display.return_value.__enter__.side_effect = (
            RuntimeError('Xvfb did not start within 10s (exit code 1).')
        )

        with tempfile.TemporaryDirectory() as tmp_dir:
            results_path = os.path.join(tmp_dir, 'output.json')
            reporter = test_results.TestReporter(output_path=results_path)
            with (
                mock.patch.object(common, 'RECORDINGS_DIR', f'{tmp_dir}/rec'),
                mock.patch.object(common, 'TRACES_DIR', f'{tmp_dir}/traces'),
                mock.patch.object(common, 'verify_receiver_connectivity'),
                mock.patch.object(
                    common, 'make_sender', return_value=fake_sender
                ),
                mock.patch.object(
                    cast_test, 'should_run_vp9_tests', return_value=True
                ),
                mock.patch.object(
                    cast_test.subprocess, 'run', return_value=completed()
                ),
                mock.patch.object(
                    cast_test.sys,
                    'argv',
                    [
                        'openscreen_cast_performance_test.py',
                        '--sender=localhost',
                        '--sender-os=linux',
                        '--receiver=sink',
                    ],
                ),
            ):
                with self.assertRaisesRegex(
                    RuntimeError, 'Xvfb did not start on the lab host'
                ):
                    with reporter:
                        cast_test._run_tests(reporter)

            with open(results_path, encoding='utf-8') as f:
                results_json = json.load(f)

        self.assertNotIn(test_results.HARNESS_CASE, results_json['tests'])
        for video in (
            '1080p30fpsH264_foodmarket_yt_sync.mp4',
            '1080p60fpsH264_boat_yt_sync.mp4',
        ):
            self.assertEqual(
                results_json['tests']['H264'][video]['actual'], 'SKIP'
            )
            self.assertNotIn(
                'is_unexpected', results_json['tests']['H264'][video]
            )
            for codec in ('VP8', 'VP9'):
                entry = results_json['tests'][codec][video]
                self.assertEqual(entry['actual'], 'FAIL')
                self.assertIn(
                    'Xvfb did not start on the lab host',
                    entry['failure_reason'],
                )

    def test_chromecast_hd_receiver_skips_60fps_vp8_clip_only(self):
        ran_videos = []

        def fake_setup(_args, _cv, _opts, codec_name, video_name):
            trace = f'/remote/{codec_name}_{video_name}_sender.perfetto-trace'
            return cast_test.SenderSession(
                mock.MagicMock(), mock.MagicMock(), '160.0.0.1', trace
            )

        def fake_run(video_name, _driver, codec_name, _args):
            ran_videos.append(f'{codec_name}_{video_name}')
            return mock.MagicMock()

        def fake_subprocess_run(cmd, **_kwargs):
            if cmd[-2:] == ['getprop', 'ro.product.model']:
                return completed(stdout='Chromecast HD\n')
            return completed()

        fake_sender = mock.MagicMock()
        fake_sender.codec_skip_reason.return_value = None
        with tempfile.TemporaryDirectory() as tmp_dir:
            results_path = os.path.join(tmp_dir, 'output.json')
            reporter = test_results.TestReporter(output_path=results_path)
            with (
                mock.patch.object(common, 'RECORDINGS_DIR', f'{tmp_dir}/rec'),
                mock.patch.object(common, 'TRACES_DIR', f'{tmp_dir}/traces'),
                mock.patch.object(common, 'verify_receiver_connectivity'),
                mock.patch.object(
                    common, 'make_sender', return_value=fake_sender
                ),
                mock.patch.object(common, 'dump_remote_logs'),
                mock.patch.object(common, 'teardown_recording_process'),
                mock.patch.object(common, 'teardown_test_environment'),
                mock.patch.object(common, 'cleanup_binaries'),
                mock.patch.object(common, 'finalize_results'),
                mock.patch.object(
                    cast_test, 'should_run_vp9_tests', return_value=True
                ),
                mock.patch.object(
                    cast_test, 'setup_test_environment', side_effect=fake_setup
                ),
                mock.patch.object(
                    cast_test, 'run_performance_test', side_effect=fake_run
                ),
                mock.patch.object(
                    cast_test.subprocess, 'run', side_effect=fake_subprocess_run
                ),
                mock.patch.object(cast_test.time, 'sleep'),
                mock.patch.object(
                    cast_test.sys,
                    'argv',
                    [
                        'openscreen_cast_performance_test.py',
                        '--sender=127.0.0.1',
                        '--sender-os=win',
                        '--receiver=sink',
                    ],
                ),
            ):
                with reporter:
                    cast_test._run_tests(reporter)

            with open(results_path, encoding='utf-8') as f:
                results_json = json.load(f)

        # H264 (30+60), VP8 (30 only), VP9 (30+60) -> 5 clips ran, 1 skipped.
        self.assertEqual(len(ran_videos), 5)
        self.assertNotIn(
            'VP8_1080p60fpsH264_boat_yt_sync.mp4',
            ran_videos,
        )
        self.assertEqual(
            results_json['tests']['VP8'][
                '1080p30fpsH264_foodmarket_yt_sync.mp4'
            ]['actual'],
            'PASS',
        )
        self.assertEqual(
            results_json['tests']['VP8']['1080p60fpsH264_boat_yt_sync.mp4'][
                'actual'
            ],
            'SKIP',
        )
        self.assertFalse(
            results_json['tests']['VP8']['1080p60fpsH264_boat_yt_sync.mp4'].get(
                'is_unexpected', False
            )
        )


class SetupTestEnvironmentTest(unittest.TestCase):
    def test_desktop_uses_trace_startup_value_instead_of_unknown_flag(self):
        fake_sender = mock.MagicMock(TMP_DIR='C:/cft_temp')
        fake_sender.install_chrome.return_value = (
            'C:/cft_temp/chrome-win64/chrome.exe',
            '159.0.0.1',
        )
        fake_sender.chrome_binary_path.return_value = (
            'C:/cft_temp/chrome-win64/chrome.exe'
        )
        captured_opts = []

        def fake_connect(opts):
            captured_opts.append(opts)
            return mock.MagicMock()

        with (
            mock.patch.object(common, 'make_sender', return_value=fake_sender),
            mock.patch.object(
                common, 'connect_to_remote_driver', side_effect=fake_connect
            ),
            mock.patch.object(cast_test, 'enable_tab_mirroring'),
        ):
            _, _, _, trace_path = cast_test.setup_test_environment(
                make_args(sender_os='win'),
                None,
                ['--v=1'],
                'VP9',
                'clip.mp4',
            )

        self.assertEqual(
            trace_path, 'C:/cft_temp/VP9_clip.mp4_sender.perfetto-trace'
        )
        args = captured_opts[0].arguments
        self.assertIn(
            f'--trace-startup={cast_test.SENDER_TRACE_CATEGORIES}', args
        )
        self.assertIn('--trace-startup-record-mode=record-continuously', args)
        self.assertFalse(
            any(a.startswith('--trace-startup-categories') for a in args)
        )

    def test_desktop_adds_sender_specific_switches(self):
        fake_sender = mock.MagicMock(TMP_DIR='/tmp', LOG_DIR=None)
        fake_sender.install_chrome.return_value = ('/tmp/c/chrome', '160.0')
        fake_sender.chrome_binary_path.return_value = '/tmp/c/chrome'
        fake_sender.extra_chrome_args.return_value = ['--no-sandbox']
        captured_opts = []

        with (
            mock.patch.object(common, 'make_sender', return_value=fake_sender),
            mock.patch.object(
                common,
                'connect_to_remote_driver',
                side_effect=lambda opts: captured_opts.append(opts),
            ),
            mock.patch.object(cast_test, 'enable_tab_mirroring'),
        ):
            cast_test.setup_test_environment(
                make_args(sender='localhost', sender_os='linux'),
                None,
                ['--v=1'],
                'VP8',
                'clip.mp4',
            )

        args = captured_opts[0].arguments
        self.assertIn('--v=1', args)
        self.assertIn('--no-sandbox', args)

    def test_cros_writes_trace_and_log_to_shared_tmp_dir(self):
        fake_sender = mock.MagicMock(
            TMP_DIR='/usr/local/tmp', LOG_DIR='/var/log/chrome'
        )
        captured_flags = []

        def fake_cros_setup(_args, _cv, opts):
            captured_flags.extend(opts)
            return mock.MagicMock(), mock.MagicMock(), '144.0.0.1'

        with (
            mock.patch.object(common, 'make_sender', return_value=fake_sender),
            mock.patch.object(
                common, 'setup_cros_environment', side_effect=fake_cros_setup
            ),
            mock.patch.object(cast_test, 'enable_tab_mirroring'),
        ):
            _, _, _, trace_path = cast_test.setup_test_environment(
                make_args(sender_os='cros'),
                None,
                ['--v=1'],
                'H264',
                'clip.mp4',
            )

        self.assertEqual(
            trace_path, '/var/log/chrome/H264_clip.mp4_sender.perfetto-trace'
        )
        self.assertIn(
            f'--trace-startup={cast_test.SENDER_TRACE_CATEGORIES}',
            captured_flags,
        )
        self.assertIn(
            '--trace-startup-file='
            '/var/log/chrome/H264_clip.mp4_sender.perfetto-trace',
            captured_flags,
        )
        self.assertIn('--trace-startup-duration=55', captured_flags)
        self.assertIn(
            '--log-file=/var/log/chrome/chrome_debug_H264_clip.mp4.log',
            captured_flags,
        )
        self.assertFalse(
            any(
                f.startswith('--trace-startup-categories')
                for f in captured_flags
            )
        )


if __name__ == '__main__':
    unittest.main()
