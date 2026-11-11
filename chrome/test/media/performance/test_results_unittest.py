#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for test_results.py and its use by the perf test scripts."""

import json
import logging
import os
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import requests

import common
from lib.results import result_sink  # On sys.path once common is imported.
import openscreen_cast_performance_test as cast_test
import openscreen_remoting_performance_test as remoting_test
from remote_transport import SenderUnreachableError
import test_results
from test_results import FailureKind, Status
import videostack_performance_test as videostack_test

UNREACHABLE = (
    "Sender 'lin-148-e504--win' is unreachable over SSH: "
    "Connection reset by peer"
)
CHROME_VERSION = '120.0.6099.0'


class FakeClock:
    """A clock that only moves when a test advances it."""

    def __init__(self):
        self.now = 1000.0

    def __call__(self):
        return self.now

    def advance(self, secs):
        self.now += secs


class FakeSink(result_sink.ResultSinkClient):
    """The real ResultSinkClient, with only its HTTP session faked.

    Using the real client means that a call it does not accept fails here, and
    `posts` holds each test result exactly as ResultSink would receive it.
    """

    def __init__(self, error=None):
        super().__init__({'address': 'localhost:0', 'auth_token': 'token'})
        self.session.close()
        self.session = mock.create_autospec(requests.Session, instance=True)
        self.session.post.side_effect = self._fake_post
        self.posts = []
        self.error = error

    def _fake_post(self, url, data):
        del url
        if self.error:
            raise self.error
        self.posts.extend(json.loads(data)['testResults'])
        return mock.create_autospec(requests.Response, instance=True)


def tag(key, value):
    """Returns a tag as it appears in a posted test result."""
    return {'key': key, 'value': value}


class ReporterTestCase(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.output_path = os.path.join(self.tmp.name, 'out', 'output.json')
        self.clock = FakeClock()
        self.sink = FakeSink()
        self.reporter = self.make_reporter(self.sink)

    def make_reporter(self, sink):
        return test_results.TestReporter(
            output_path=self.output_path, sink_client=sink, clock=self.clock
        )

    def read_output(self):
        with open(self.output_path, encoding='utf-8') as f:
            return json.load(f)

    def statuses(self):
        return {r.name: r.status for r in self.reporter.results}

    def posts_by_name(self):
        return {p['testId']: p for p in self.sink.posts}


class EnumTest(unittest.TestCase):
    def test_members_print_and_serialize_as_their_value(self):
        # The JSON file, log lines, and ResultSink tags all expect the plain
        # value, e.g. 'FAIL' rather than 'Status.FAIL'.
        self.assertEqual('FAIL', str(Status.FAIL))
        self.assertEqual('infra', f'{FailureKind.INFRA}')
        self.assertEqual(
            '["SKIP", "test"]', json.dumps([Status.SKIP, FailureKind.TEST])
        )
        self.assertEqual(Status.PASS, 'PASS')


class TestReporterTest(ReporterTestCase):
    def test_passing_case_reports_its_duration(self):
        with self.reporter:
            with self.reporter.case('H264/a.mp4'):
                self.clock.advance(2.5)

        self.assertEqual(self.statuses(), {'H264/a.mp4': Status.PASS})
        (post,) = self.sink.posts
        self.assertEqual(post['status'], 'PASS')
        self.assertEqual(post['duration'], '2.500000000s')
        self.assertTrue(post['expected'])
        self.assertNotIn('failureReason', post)
        self.assertEqual(
            post['testIdStructured']['caseNameComponents'], ['H264', 'a.mp4']
        )
        self.assertEqual(
            self.read_output()['tests']['H264']['a.mp4']['time'], 2.5
        )

    def test_failing_case_records_reason_and_reraises(self):
        with (
            self.assertLogs(level='ERROR') as logs,
            self.assertRaises(ValueError),
            self.reporter,
            self.reporter.case('H264/a.mp4'),
        ):
            raise ValueError('frame rate too low')

        (result,) = self.reporter.results
        self.assertEqual(result.status, Status.FAIL)
        self.assertEqual(result.reason, 'ValueError: frame rate too low')
        self.assertEqual(result.kind, FailureKind.TEST)
        self.assertIn(
            'ERROR:root:FAILED H264/a.mp4 [test]: ValueError: frame rate too '
            'low',
            logs.output,
        )
        (post,) = self.sink.posts
        self.assertEqual(post['status'], 'FAIL')
        self.assertFalse(post['expected'])
        self.assertEqual(
            post['failureReason']['primaryErrorMessage'], result.reason
        )
        self.assertIn(tag('failure_kind', 'test'), post['tags'])

    def test_remote_device_error_is_an_infra_failure(self):
        with (
            self.assertLogs(level='ERROR'),
            self.assertRaises(SenderUnreachableError),
            self.reporter,
            self.reporter.case(test_results.SETUP_CASE),
        ):
            raise SenderUnreachableError(UNREACHABLE)

        (result,) = self.reporter.results
        self.assertEqual(result.kind, FailureKind.INFRA)
        self.assertIn('Connection reset by peer', result.reason)
        self.assertIn(tag('failure_kind', 'infra'), self.sink.posts[0]['tags'])

    def test_wrapped_errors_name_their_root_cause(self):
        with (
            self.assertLogs(level='ERROR'),
            self.assertRaises(RuntimeError),
            self.reporter,
            self.reporter.case('setup'),
        ):
            try:
                raise SenderUnreachableError(UNREACHABLE)
            except SenderUnreachableError as e:
                raise RuntimeError('Could not connect to chromedriver.') from e

        (result,) = self.reporter.results
        self.assertEqual(result.kind, FailureKind.INFRA)
        self.assertEqual(
            result.reason,
            'RuntimeError: Could not connect to chromedriver. (caused by '
            f'SenderUnreachableError: {UNREACHABLE})',
        )

    def test_planned_cases_that_did_not_run_name_the_failure(self):
        self.reporter.plan(['setup', 'H264/a.mp4', 'VP8/a.mp4'])
        with (
            self.assertLogs(level='ERROR'),
            self.assertRaises(SenderUnreachableError),
            self.reporter,
            self.reporter.case('setup'),
        ):
            raise SenderUnreachableError(UNREACHABLE)

        self.assertEqual(
            self.statuses(),
            {
                'setup': Status.FAIL,
                'H264/a.mp4': Status.SKIP,
                'VP8/a.mp4': Status.SKIP,
            },
        )
        not_run = self.reporter.results[1]
        self.assertFalse(not_run.expected)
        self.assertEqual(not_run.kind, FailureKind.INFRA)
        self.assertTrue(
            not_run.reason.startswith('Not run: setup failed: '),
            not_run.reason,
        )
        post = self.posts_by_name()['H264/a.mp4']
        self.assertEqual(post['status'], 'SKIP')
        # ResultSink cannot mark a skip unexpected; the tag tells it apart
        # from a deliberate skip.
        self.assertTrue(post['expected'])
        self.assertIn(tag('failure_kind', 'infra'), post['tags'])
        self.assertIn('Not run: setup failed: ', post['summaryHtml'])

    def test_planned_cases_missing_after_a_clean_exit_are_unexpected(self):
        self.reporter.plan(['a', 'b'])
        with self.reporter:
            with self.reporter.case('a'):
                pass

        output = self.read_output()
        self.assertFalse(output['interrupted'])
        self.assertEqual(
            output['tests']['b'],
            {
                'expected': Status.PASS,
                'actual': Status.SKIP,
                'time': 0.0,
                'is_unexpected': True,
                'skip_reason': 'Not run.',
                'failure_kind': 'test',
            },
        )

    def test_deliberate_skips_are_expected_and_escaped(self):
        with self.reporter:
            self.reporter.skip('VP9/a.mp4', 'No <VP9> & co.')
            with self.reporter.case('H264/a.mp4') as case:
                case.skip('Video did not load.')

        self.assertEqual(
            self.statuses(),
            {'VP9/a.mp4': Status.SKIP, 'H264/a.mp4': Status.SKIP},
        )
        self.assertTrue(all(r.expected for r in self.reporter.results))
        post = self.posts_by_name()['VP9/a.mp4']
        self.assertEqual(post['status'], 'SKIP')
        self.assertTrue(post['expected'])
        self.assertNotIn('failureReason', post)
        self.assertNotIn('failure_kind', [t['key'] for t in post['tags']])
        self.assertEqual(post['summaryHtml'], 'No &lt;VP9&gt; &amp; co.')

    def test_error_outside_a_case_is_reported_as_harness(self):
        with (
            self.assertLogs(level='ERROR'),
            self.assertRaises(RuntimeError),
            self.reporter,
        ):
            raise RuntimeError('upload failed')

        (result,) = self.reporter.results
        self.assertEqual(result.name, test_results.HARNESS_CASE)
        self.assertEqual(result.reason, 'RuntimeError: upload failed')

    def test_case_error_is_not_reported_again_as_harness(self):
        with (
            self.assertLogs(level='ERROR'),
            self.assertRaises(ValueError),
            self.reporter,
            self.reporter.case('H264/a.mp4'),
        ):
            raise ValueError('boom')

        self.assertEqual(self.statuses(), {'H264/a.mp4': Status.FAIL})

    def test_interrupt_fails_the_running_case(self):
        self.reporter.plan(['a', 'b'])
        with (
            self.assertLogs(level='ERROR'),
            self.assertRaises(KeyboardInterrupt),
            self.reporter,
            self.reporter.case('a'),
        ):
            raise KeyboardInterrupt()

        output = self.read_output()
        self.assertTrue(output['interrupted'])
        self.assertEqual(sorted(output['tests']), ['a', 'b'])
        self.assertEqual(output['tests']['a']['actual'], Status.FAIL)
        self.assertEqual(output['tests']['a']['failure_kind'], 'infra')
        self.assertTrue(
            output['tests']['b']['skip_reason'].startswith(
                'Not run: a failed: KeyboardInterrupt'
            )
        )

    def test_a_case_cannot_be_reported_twice(self):
        body = mock.Mock()
        with self.reporter:
            self.reporter.skip('VP9/a.mp4', 'No VP9 support.')
            with self.assertRaises(ValueError):
                with self.reporter.case('VP9/a.mp4'):
                    body()

        body.assert_not_called()
        self.assertEqual(self.statuses(), {'VP9/a.mp4': Status.SKIP})

    def test_names_must_form_a_valid_results_trie(self):
        with self.reporter:
            self.reporter.skip('H264', 'leaf')
            for name in ('H264/a.mp4', 'VP8//a.mp4', '/a.mp4'):
                with self.subTest(name=name), self.assertRaises(ValueError):
                    self.reporter.skip(name, 'conflicts')

        self.assertEqual(self.statuses(), {'H264': Status.SKIP})

    def test_long_reasons_are_truncated_where_reported(self):
        with (
            self.assertLogs(level='ERROR'),
            self.assertRaises(ValueError),
            self.reporter,
            self.reporter.case('a'),
        ):
            raise ValueError('x' * 5000)

        for reason in (
            self.sink.posts[0]['failureReason']['primaryErrorMessage'],
            self.read_output()['tests']['a']['failure_reason'],
        ):
            self.assertLessEqual(len(reason), 1000)
            self.assertTrue(reason.endswith('...'))

    def test_sink_errors_do_not_stop_the_run(self):
        for error in (requests.ConnectionError('down'), OSError('closed')):
            with self.subTest(error=error):
                self.reporter = self.make_reporter(FakeSink(error=error))
                with self.assertLogs(level='ERROR') as logs, self.reporter:
                    for name in ('a', 'b'):
                        with self.reporter.case(name):
                            pass

                self.assertEqual(
                    self.read_output()['num_failures_by_type'][Status.PASS], 2
                )
                self.assertTrue(
                    any(
                        'Failed to report a to ResultSink' in line
                        for line in logs.output
                    ),
                    logs.output,
                )

    def test_json_write_errors_do_not_mask_the_failure(self):
        blocker = os.path.join(self.tmp.name, 'file')
        with open(blocker, 'w', encoding='utf-8'):
            pass
        self.output_path = os.path.join(blocker, 'output.json')
        self.reporter = self.make_reporter(None)
        with (
            self.assertLogs(level='ERROR') as logs,
            self.assertRaises(ValueError),
            self.reporter,
            self.reporter.case('a'),
        ):
            raise ValueError('boom')

        self.assertTrue(
            any('Failed to write test results' in line for line in logs.output)
        )


class JsonOutputTest(ReporterTestCase):
    def test_writes_json_test_results_even_when_the_run_fails(self):
        self.reporter.plan(['H264/a.mp4', 'H264/b.mp4'])
        with (
            self.assertLogs(level='ERROR'),
            self.assertRaises(ValueError),
            self.reporter,
            self.reporter.case('H264/a.mp4'),
        ):
            self.clock.advance(1.0)
            raise ValueError('boom')

        output = self.read_output()
        self.assertEqual(output['version'], 3)
        self.assertTrue(output['interrupted'])
        self.assertEqual(output['path_delimiter'], '/')
        self.assertEqual(output['seconds_since_epoch'], 1000.0)
        self.assertEqual(
            output['num_failures_by_type'],
            {Status.PASS: 0, Status.FAIL: 1, Status.SKIP: 1},
        )
        self.assertEqual(
            output['tests']['H264']['a.mp4'],
            {
                'expected': Status.PASS,
                'actual': Status.FAIL,
                'time': 1.0,
                'is_unexpected': True,
                'failure_reason': 'ValueError: boom',
                'failure_kind': 'test',
            },
        )
        not_run = output['tests']['H264']['b.mp4']
        self.assertEqual(not_run['actual'], Status.SKIP)
        self.assertEqual(not_run['expected'], Status.PASS)
        self.assertTrue(not_run['is_unexpected'])

    def test_expected_results_are_not_marked_unexpected(self):
        with self.reporter:
            with self.reporter.case('setup'):
                pass
            self.reporter.skip('VP9/a.mp4', 'No VP9 support.')

        tests = self.read_output()['tests']
        self.assertEqual(
            tests['setup'],
            {'expected': Status.PASS, 'actual': Status.PASS, 'time': 0.0},
        )
        self.assertEqual(tests['VP9']['a.mp4']['expected'], Status.SKIP)
        self.assertNotIn('is_unexpected', tests['VP9']['a.mp4'])

    def test_from_argv_reads_the_isolated_script_output_path(self):
        with mock.patch.object(
            test_results.result_sink, 'TryInitClient', return_value=None
        ):
            reporter = test_results.TestReporter.from_argv(
                [
                    '--sender',
                    'x',
                    f'--isolated-script-test-output={self.output_path}',
                ]
            )
        with reporter:
            pass

        self.assertEqual(self.read_output()['tests'], {})


class ScriptWiringTest(unittest.TestCase):
    """Runs each script's main() with its sender and receiver faked out."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.output_path = os.path.join(self.tmp.name, 'output.json')
        for name, value in (
            ('RECORDINGS_DIR', os.path.join(self.tmp.name, 'recordings')),
            ('TRACES_DIR', os.path.join(self.tmp.name, 'traces')),
        ):
            self.patch(common, name, value)
        self.finalize_results = self.patch(common, 'finalize_results')
        for name in (
            'cleanup_binaries',
            'teardown_test_environment',
            'teardown_recording_process',
            'dump_remote_logs',
            'verify_receiver_connectivity',
        ):
            self.patch(common, name)
        self.patch(test_results.result_sink, 'TryInitClient', return_value=None)
        # The scripts' best-effort adb setup.
        adb_ok = subprocess.CompletedProcess([], 0, stdout='', stderr='')
        self.patch(cast_test.subprocess, 'run', return_value=adb_ok)
        self.patch(remoting_test.subprocess, 'run', return_value=adb_ok)
        # main() raises the root logger to INFO.
        root = logging.getLogger()
        self.addCleanup(root.setLevel, root.level)
        self.logs = []

    def patch(self, target, name, *args, **kwargs):
        patcher = mock.patch.object(target, name, *args, **kwargs)
        self.addCleanup(patcher.stop)
        return patcher.start()

    def run_main(self, module, *argv, raises=None):
        """Runs module.main(), capturing its logs in self.logs.

        Returns:
            The JSON test results it wrote.
        """
        argv = [
            module.__file__,
            *argv,
            f'--isolated-script-test-output={self.output_path}',
        ]
        with (
            mock.patch.object(sys, 'argv', argv),
            self.assertLogs(level='INFO') as logs,
        ):
            if raises:
                with self.assertRaises(raises):
                    module.main()
            else:
                module.main()
        self.logs = logs.output
        with open(self.output_path, encoding='utf-8') as f:
            return json.load(f)

    def run_main_unreachable(self, module, *argv):
        self.patch(
            module,
            'setup_test_environment',
            side_effect=SenderUnreachableError(UNREACHABLE),
        )
        output = self.run_main(module, *argv, raises=SenderUnreachableError)
        self.assertTrue(
            any('FAILED' in line and '[infra]' in line for line in self.logs)
        )
        return output

    def assert_reason(self, entry, prefix=''):
        key = (
            'failure_reason'
            if entry['actual'] == Status.FAIL
            else 'skip_reason'
        )
        self.assertTrue(entry[key].startswith(prefix), entry)
        self.assertIn('Connection reset by peer', entry[key])
        self.assertEqual(entry['failure_kind'], 'infra')

    def cast_session(self):
        return cast_test.SenderSession(
            mock.Mock(), mock.Mock(), CHROME_VERSION, '/tmp/trace'
        )

    def test_cast_unreachable(self):
        self.patch(cast_test, 'should_run_vp9_tests', return_value=False)
        output = self.run_main_unreachable(
            cast_test, '--sender', 'win-host', '--sender-os', 'win'
        )

        tests = output['tests']
        self.assertEqual(sorted(tests), ['H264', 'VP8', 'VP9'])
        video = '1080p30fpsH264_foodmarket_yt_sync.mp4'
        self.assertEqual(tests['H264'][video]['actual'], Status.FAIL)
        self.assert_reason(tests['H264'][video], 'SenderUnreachableError: ')
        self.assertEqual(tests['VP8'][video]['actual'], Status.SKIP)
        self.assert_reason(tests['VP8'][video], 'Not run: H264/')
        # VP9 is a deliberate skip, not a casualty of the failure.
        self.assertEqual(tests['VP9'][video]['expected'], Status.SKIP)

    def test_cast_passes(self):
        self.patch(cast_test, 'should_run_vp9_tests', return_value=True)
        self.patch(
            cast_test,
            'setup_test_environment',
            return_value=self.cast_session(),
        )
        self.patch(cast_test, 'run_performance_test')
        self.patch(cast_test, 'collect_sender_trace')

        output = self.run_main(
            cast_test, '--sender', 'win-host', '--sender-os', 'win'
        )

        self.assertFalse(output['interrupted'])
        self.assertEqual(
            output['num_failures_by_type'],
            {Status.PASS: 6, Status.FAIL: 0, Status.SKIP: 0},
        )
        self.finalize_results.assert_called_once_with(CHROME_VERSION)

    def test_cast_failure_after_setup_keeps_the_chrome_version(self):
        self.patch(cast_test, 'should_run_vp9_tests', return_value=False)
        self.patch(
            cast_test,
            'setup_test_environment',
            return_value=self.cast_session(),
        )
        self.patch(
            cast_test,
            'run_performance_test',
            side_effect=RuntimeError('Failed to start tab mirroring.'),
        )
        self.patch(cast_test, 'collect_sender_trace')

        self.run_main(
            cast_test,
            '--sender',
            'win-host',
            '--sender-os',
            'win',
            raises=RuntimeError,
        )

        self.finalize_results.assert_called_once_with(CHROME_VERSION)

    def test_remoting_unreachable(self):
        self.patch(remoting_test, 'install_chrome_on_android')
        output = self.run_main_unreachable(
            remoting_test,
            '--receiver',
            'pixel',
            '--receiver-type',
            'android',
        )

        tests = output['tests']
        self.assertEqual(tests['setup']['actual'], Status.FAIL)
        self.assert_reason(tests['setup'], 'SenderUnreachableError: ')
        for video in common.VIDEOS:
            self.assertEqual(tests[video['name']]['actual'], Status.SKIP)
            self.assert_reason(tests[video['name']], 'Not run: setup failed')

    def test_remoting_on_fuchsia_reports_deliberate_skips(self):
        output = self.run_main(remoting_test, '--receiver', 'fuchsia')

        tests = output['tests']
        self.assertEqual(
            sorted(tests), sorted(v['name'] for v in common.VIDEOS)
        )
        for entry in tests.values():
            self.assertEqual(entry['expected'], Status.SKIP)
            self.assertIn('b/502641691', entry['skip_reason'])

    def test_videostack_unreachable(self):
        output = self.run_main_unreachable(
            videostack_test, '--sender', 'mac-host', '--sender-os', 'mac'
        )

        tests = output['tests']
        self.assertEqual(tests['setup']['actual'], Status.FAIL)
        self.assert_reason(tests['setup'], 'SenderUnreachableError: ')
        self.assertEqual(len(tests), 1 + len(common.VIDEOS))

    def test_videostack_cros_skips_hevc_deliberately(self):
        output = self.run_main_unreachable(
            videostack_test, '--sender', 'dut', '--sender-os', 'cros'
        )

        hevc = output['tests']['1080p60fpsHEVC_boat_yt_sync.mp4']
        self.assertEqual(hevc['actual'], Status.SKIP)
        self.assertEqual(hevc['expected'], Status.SKIP)
        self.assertIn('b/512198717', hevc['skip_reason'])

    def test_videostack_upload_failure_is_reported_as_harness(self):
        self.patch(
            videostack_test,
            'setup_test_environment',
            return_value=(mock.Mock(), mock.Mock(), CHROME_VERSION),
        )
        self.patch(videostack_test, 'run_performance_test')
        self.finalize_results.side_effect = RuntimeError('upload failed')

        tests = self.run_main(
            videostack_test,
            '--sender',
            'mac-host',
            '--sender-os',
            'mac',
            raises=RuntimeError,
        )['tests']
        self.assertEqual(tests['harness']['actual'], Status.FAIL)
        self.assertEqual(
            tests['harness']['failure_reason'], 'RuntimeError: upload failed'
        )
        for video in common.VIDEOS:
            self.assertEqual(tests[video['name']]['actual'], Status.PASS)


if __name__ == '__main__':
    unittest.main()
