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
from perf_test_fakes import completed
import test_results


class TraceConfigTest(unittest.TestCase):
    def test_receiver_perfetto_config_enables_cast_categories(self):
        cfg = cast_test.PERFETTO_CONFIG
        self.assertIn('name: "track_event"', cfg)
        self.assertIn('name: "org.chromium.trace_event"', cfg)
        for cat in ('openscreen', 'cast.stream', 'media'):
            self.assertIn(f'enabled_categories: "{cat}"', cfg)
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


if __name__ == '__main__':
    unittest.main()
