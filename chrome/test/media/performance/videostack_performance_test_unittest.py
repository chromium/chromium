#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for videostack_performance_test.py."""

import json
import os
import tempfile
import unittest
from unittest import mock

import common
import test_results
import videostack_performance_test as vs_test


class VideoCatalogTest(unittest.TestCase):
    def test_videos_use_cipd_hevc_filename(self):
        names = [v['name'] for v in common.VIDEOS]
        self.assertIn('1080p60fpsHEVC_boat_sync.mp4', names)
        self.assertNotIn('1080p60fpsHEVC_boat_yt_sync.mp4', names)

    def test_missing_reference_video_fails_before_browser_or_capture(self):
        driver = mock.MagicMock()
        args = mock.MagicMock(sender_os='mac')
        with tempfile.TemporaryDirectory() as tmp_dir:
            with (
                mock.patch.object(common.server, 'VIDEO_DIR', tmp_dir),
                mock.patch.object(common, 'start_glances_monitoring') as mon,
                mock.patch.object(vs_test.subprocess, 'Popen') as popen,
            ):
                with self.assertRaises(FileNotFoundError):
                    vs_test.run_performance_test('missing.mp4', driver, args)
        driver.get.assert_not_called()
        mon.assert_not_called()
        popen.assert_not_called()


class MainLoopContinuationTest(unittest.TestCase):
    def test_later_videos_still_run_after_clip_failure(self):
        ran_videos = []

        def fake_setup(_args, _cv):
            return mock.MagicMock(), mock.MagicMock(), '160.0.8171.0'

        def fake_run_case(video_name, _driver, _args):
            ran_videos.append(video_name)
            if video_name == '1080p60fpsHEVC_boat_sync.mp4':
                raise RuntimeError('simulated analyzer failure')

        with tempfile.TemporaryDirectory() as tmp_dir:
            results_path = os.path.join(tmp_dir, 'output.json')
            reporter = test_results.TestReporter(output_path=results_path)
            with (
                mock.patch.object(common, 'RECORDINGS_DIR', f'{tmp_dir}/rec'),
                mock.patch.object(common, 'teardown_test_environment'),
                mock.patch.object(common, 'finalize_results') as finalize,
                mock.patch.object(
                    vs_test, 'setup_test_environment', side_effect=fake_setup
                ),
                mock.patch.object(
                    vs_test, 'run_video_case', side_effect=fake_run_case
                ),
                mock.patch.object(
                    vs_test.sys,
                    'argv',
                    [
                        'videostack_performance_test.py',
                        '--sender=127.0.0.1',
                        '--sender-os=mac',
                    ],
                ),
            ):
                with self.assertRaisesRegex(
                    RuntimeError, 'simulated analyzer failure'
                ):
                    with reporter:
                        vs_test._run_tests(reporter)

            with open(results_path, encoding='utf-8') as f:
                results_json = json.load(f)

        self.assertEqual(
            ran_videos,
            [
                '1080p30fpsAV1_foodmarket_sync.mp4',
                '1080p30fpsH264_foodmarket_yt_sync.mp4',
                '1080p60fpsHEVC_boat_sync.mp4',
                '1080p60fpsVP9_boat_yt_sync.webm',
            ],
        )
        finalize.assert_called_once_with('160.0.8171.0')
        self.assertNotIn(test_results.HARNESS_CASE, results_json['tests'])
        self.assertEqual(
            results_json['tests']['1080p60fpsHEVC_boat_sync.mp4']['actual'],
            'FAIL',
        )
        self.assertEqual(
            results_json['tests']['1080p60fpsVP9_boat_yt_sync.webm']['actual'],
            'PASS',
        )

    def test_infra_error_aborts_remaining_clips_immediately(self):
        ran_videos = []

        def fake_setup(_args, _cv):
            return mock.MagicMock(), mock.MagicMock(), '160.0.8171.0'

        def fake_run_case(video_name, _driver, _args):
            ran_videos.append(video_name)
            raise common.SenderUnreachableError('ssh lost')

        with tempfile.TemporaryDirectory() as tmp_dir:
            results_path = os.path.join(tmp_dir, 'output.json')
            reporter = test_results.TestReporter(output_path=results_path)
            with (
                mock.patch.object(common, 'RECORDINGS_DIR', f'{tmp_dir}/rec'),
                mock.patch.object(common, 'teardown_test_environment'),
                mock.patch.object(common, 'finalize_results'),
                mock.patch.object(
                    vs_test, 'setup_test_environment', side_effect=fake_setup
                ),
                mock.patch.object(
                    vs_test, 'run_video_case', side_effect=fake_run_case
                ),
                mock.patch.object(
                    vs_test.sys,
                    'argv',
                    [
                        'videostack_performance_test.py',
                        '--sender=127.0.0.1',
                        '--sender-os=mac',
                    ],
                ),
            ):
                with self.assertRaises(common.SenderUnreachableError):
                    with reporter:
                        vs_test._run_tests(reporter)

        self.assertEqual(ran_videos, ['1080p30fpsAV1_foodmarket_sync.mp4'])


if __name__ == '__main__':
    unittest.main()
