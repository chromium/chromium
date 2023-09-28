# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import plistlib
import subprocess
import unittest
from unittest import mock

from signing import notarize, test_config
from signing.model import CodeSignedProduct, Paths


class TestSubmitNotarytool(unittest.TestCase):

    @mock.patch('signing.commands.run_command_output')
    def test_valid_upload(self, run_command_output):
        run_command_output.return_value = plistlib.dumps({
            'id': '13d6aa9b-d204-4f0d-9164-4bda5e730258',
            'message': 'Successfully uploaded file',
            'path': '/tmp/file.dmg'
        })
        config = test_config.TestConfig()
        uuid = notarize.submit('/tmp/file.dmg', config)

        self.assertEqual('13d6aa9b-d204-4f0d-9164-4bda5e730258', uuid)
        run_command_output.assert_called_once_with([
            'xcrun', 'notarytool', 'submit', '/tmp/file.dmg', '--no-wait',
            '--output-format', 'plist'
        ])

    @mock.patch('signing.commands.run_command_output')
    def test_valid_upload_with_args(self, run_command_output):
        run_command_output.return_value = plistlib.dumps(
            {'id': 'b53b3ed1-82cb-41b4-9e12-b097b2c05f64'})
        config = test_config.TestConfig(
            invoker=test_config.TestInvoker.factory_with_args(notary_arg=[
                '--apple-id', '[NOTARY-USER]', '--team-id', '[NOTARY-TEAM]',
                '--password', 'hunter2'
            ]))
        uuid = notarize.submit('/tmp/file.dmg', config)

        self.assertEqual('b53b3ed1-82cb-41b4-9e12-b097b2c05f64', uuid)
        run_command_output.assert_called_once_with([
            'xcrun', 'notarytool', 'submit', '/tmp/file.dmg', '--no-wait',
            '--output-format', 'plist', '--apple-id', '[NOTARY-USER]',
            '--team-id', '[NOTARY-TEAM]', '--password', 'hunter2'
        ])


class TestGetResultNotarytool(unittest.TestCase):

    @mock.patch('signing.commands.run_command_output')
    def test_successs(self, run_command_output):
        plist_output = plistlib.dumps({
            'status': 'Accepted',
            'id': 'eeeacc17-9e4b-4408-8001-894bbae9c9e9',
            'createdDate': '2022-03-10T23:04:21.192Z',
            'message': 'Successfully received submission info',
            'name': 'file.zip'
        })
        run_command_output.return_value = plist_output
        uuid = 'eeeacc17-9e4b-4408-8001-894bbae9c9e9'
        config = test_config.TestConfig()
        result = config.invoker.notarizer.get_result(uuid, config)

        self.assertEqual(notarize.Status.SUCCESS, result.status)
        self.assertEqual('Accepted', result.status_string)
        self.assertEqual(plist_output, result.output)
        self.assertEqual(None, result.log_file)

        run_command_output.assert_called_once_with(
            ['xcrun', 'notarytool', 'info', uuid, '--output-format', 'plist'])

    @mock.patch('signing.commands.run_command_output')
    def test_success_with_args(self, run_command_output):
        plist_output = plistlib.dumps({
            'status': 'Accepted',
            'id': 'eeeacc17-9e4b-4408-8001-894bbae9c9e9',
            'createdDate': '2022-03-10T23:04:21.192Z',
            'message': 'Successfully received submission info',
            'name': 'file.zip'
        })
        run_command_output.return_value = plist_output
        uuid = 'eeeacc17-9e4b-4408-8001-894bbae9c9e9'
        os.environ['NOTARIZE_TEST_PASSWORD'] = 'hunter2'
        config = test_config.TestConfig(
            invoker=test_config.TestInvoker.factory_with_args(notary_arg=[
                '--apple-id', '[NOTARY-USER]', '--team-id', '[NOTARY-TEAM]',
                '--password', 'hunter2'
            ]))
        result = config.invoker.notarizer.get_result(uuid, config)

        self.assertEqual(notarize.Status.SUCCESS, result.status)
        self.assertEqual('Accepted', result.status_string)
        self.assertEqual(plist_output, result.output)
        self.assertEqual(None, result.log_file)

        run_command_output.assert_called_once_with([
            'xcrun', 'notarytool', 'info', uuid, '--output-format', 'plist',
            '--apple-id', '[NOTARY-USER]', '--team-id', '[NOTARY-TEAM]',
            '--password', 'hunter2'
        ])

    @mock.patch('signing.commands.run_command_output')
    def test_in_progress(self, run_command_output):
        plist_output = plistlib.dumps({
            'status': 'In Progress',
            'id': 'eeeacc17-9e4b-4408-8001-894bbae9c9e9',
            'createdDate': '2022-03-10T23:04:21.192Z',
            'message': 'Successfully received submission info',
            'name': 'file.zip'
        })
        run_command_output.return_value = plist_output
        uuid = 'eeeacc17-9e4b-4408-8001-894bbae9c9e9'
        config = test_config.TestConfig()
        result = config.invoker.notarizer.get_result(uuid, config)

        self.assertEqual(notarize.Status.IN_PROGRESS, result.status)
        self.assertEqual('In Progress', result.status_string)
        self.assertEqual(plist_output, result.output)
        self.assertEqual(None, result.log_file)

        run_command_output.assert_called_once_with(
            ['xcrun', 'notarytool', 'info', uuid, '--output-format', 'plist'])

    @mock.patch('signing.commands.run_command_output')
    def test_rejected(self, run_command_output):
        plist_output = plistlib.dumps({
            'status': 'Invalid',
            'id': '13d6aa9b-d204-4f0d-9164-4bda5e730258',
            'createdDate': '2022-03-10T21:28:01.741Z',
            'message': 'Successfully received submission info',
            'name': 'chromium_helper.zip'
        })
        run_command_output.side_effect = [
            plist_output, b'This is the log file contents'
        ]
        uuid = '13d6aa9b-d204-4f0d-9164-4bda5e730258'
        config = test_config.TestConfig()
        result = config.invoker.notarizer.get_result(uuid, config)

        self.assertEqual(notarize.Status.ERROR, result.status)
        self.assertEqual('Invalid', result.status_string)
        self.assertEqual(plist_output, result.output)
        self.assertEqual('This is the log file contents', result.log_file)

        run_command_output.assert_has_calls([
            mock.call([
                'xcrun', 'notarytool', 'info', uuid, '--output-format', 'plist'
            ]),
            mock.call(['xcrun', 'notarytool', 'log', uuid])
        ])

    @mock.patch('signing.commands.run_command_output')
    def test_rejected_failed_get_log(self, run_command_output):
        plist_output = plistlib.dumps({
            'status': 'Invalid',
            'id': '13d6aa9b-d204-4f0d-9164-4bda5e730258',
            'createdDate': '2022-03-10T21:28:01.741Z',
            'message': 'Successfully received submission info',
            'name': 'chromium_helper.zip'
        })
        run_command_output.side_effect = [
            plist_output,
            subprocess.CalledProcessError(1, 'notarytool', 'Error message.')
        ]
        uuid = '13d6aa9b-d204-4f0d-9164-4bda5e730258'
        config = test_config.TestConfig()
        result = config.invoker.notarizer.get_result(uuid, config)

        self.assertEqual(notarize.Status.ERROR, result.status)
        self.assertEqual('Invalid', result.status_string)
        self.assertEqual(plist_output, result.output)
        self.assertEqual(None, result.log_file)

        run_command_output.assert_has_calls([
            mock.call([
                'xcrun', 'notarytool', 'info', uuid, '--output-format', 'plist'
            ]),
            mock.call(['xcrun', 'notarytool', 'log', uuid])
        ])


class TestWaitForResults(unittest.TestCase):

    @mock.patch('signing.notarize.Invoker.get_result')
    def test_success(self, get_result):
        get_result.return_value = notarize.NotarizationResult(
            notarize.Status.SUCCESS, 'success', None,
            'https://example.com/log.json')
        config = test_config.TestConfig()
        uuid = 'cca0aec2-7c64-4ea4-b895-051ea3a17311'
        uuids = [uuid]
        self.assertEqual(uuids, list(notarize.wait_for_results(uuids, config)))
        get_result.assert_called_once_with(uuid, config)

    @mock.patch('signing.notarize.Invoker.get_result')
    def test_failure(self, get_result):
        get_result.return_value = notarize.NotarizationResult(
            notarize.Status.ERROR, 'invalid', 'Package Invalid',
            'https://example.com/log.json')
        config = test_config.TestConfig()
        uuid = 'cca0aec2-7c64-4ea4-b895-051ea3a17311'
        uuids = [uuid]
        with self.assertRaises(notarize.NotarizationError) as cm:
            list(notarize.wait_for_results(uuids, config))

        self.assertEqual(
            'Notarization request cca0aec2-7c64-4ea4-b895-051ea3a17311 failed '
            'with status: "invalid".', str(cm.exception))

    @mock.patch('signing.notarize.Invoker.get_result')
    def test_subprocess_errors(self, get_result):
        get_result.side_effect = subprocess.CalledProcessError(
            1, 'notarytool', 'A mysterious error occurred.')

        with self.assertRaises(subprocess.CalledProcessError):
            uuids = ['77c0ad17-479e-4b82-946a-73739cf6ca16']
            list(notarize.wait_for_results(uuids, test_config.TestConfig()))

    @mock.patch.multiple('time', **{'sleep': mock.DEFAULT})
    @mock.patch.multiple('signing.notarize.Invoker',
                         **{'get_result': mock.DEFAULT})
    def test_timeout(self, **kwargs):
        kwargs['get_result'].return_value = notarize.NotarizationResult(
            notarize.Status.IN_PROGRESS, None, None, None)
        config = test_config.TestConfig()
        uuid = '0c652bb4-7d44-4904-8c59-1ee86a376ece'
        uuids = [uuid]
        with self.assertRaises(notarize.NotarizationError) as cm:
            list(notarize.wait_for_results(uuids, config))

        # Python 2 and 3 stringify set() differently.
        self.assertIn(
            str(cm.exception), [
                "Timed out waiting for notarization requests: set(['0c652bb4-7d44-4904-8c59-1ee86a376ece'])",
                "Timed out waiting for notarization requests: {'0c652bb4-7d44-4904-8c59-1ee86a376ece'}"
            ])

        for call in kwargs['get_result'].mock_calls:
            self.assertEqual(call, mock.call(uuid, config))

        total_time = sum([call[1][0] for call in kwargs['sleep'].mock_calls])
        self.assertLess(total_time, 61 * 60)


class TestStaple(unittest.TestCase):

    @mock.patch('signing.commands.run_command')
    def test_staple(self, run_command):
        notarize.staple('/tmp/file.dmg')
        run_command.assert_called_once_with(
            ['xcrun', 'stapler', 'staple', '--verbose', '/tmp/file.dmg'])

    @mock.patch('signing.notarize.staple')
    def test_staple_bundled_parts(self, staple):
        notarize.staple_bundled_parts([
            CodeSignedProduct('Foo.app/Contents/Helpers/Helper.app', ''),
            CodeSignedProduct('Foo.app/Contents/Helpers/loose_exectuable', ''),
            CodeSignedProduct('Foo.app/Contents/XPCServices/Service1.xpc', ''),
            CodeSignedProduct(
                'Foo.app/Contents/Helpers/Helper.app/Contents/Helpers/Bar.app',
                ''),
            CodeSignedProduct(
                'Foo.app/Contents/Helpers/Helper.app/Contents/XPCServices/'
                'Service2.xpc', ''),
            CodeSignedProduct('Foo.app', '')
        ], Paths('/in', '/out', '/work'))
        staple.assert_has_calls([
            mock.call('/work/Foo.app/Contents/Helpers/Helper.app/Contents'
                      '/Helpers/Bar.app'),
            mock.call('/work/Foo.app/Contents/Helpers/Helper.app'),
            mock.call('/work/Foo.app')
        ])

    @mock.patch.multiple('time', **{'sleep': mock.DEFAULT})
    @mock.patch('signing.commands.run_command')
    def test_fail_once_then_succeed(self, run_command, **kwargs):
        run_command.side_effect = [
            subprocess.CalledProcessError(
                65, 'stapler',
                'CloudKit query for [file] ([hash]) failed due to "(null)"'),
            None
        ]
        notarize.staple('/tmp/file.dmg')
        run_command.assert_has_calls(2 * [
            mock.call(
                ['xcrun', 'stapler', 'staple', '--verbose', '/tmp/file.dmg'])
        ])
        self.assertEqual(1, kwargs['sleep'].call_count)

    @mock.patch.multiple('time', **{'sleep': mock.DEFAULT})
    @mock.patch('signing.commands.run_command')
    def test_fail_twice_with_unexpected_code(self, run_command, **kwargs):
        run_command.side_effect = [
            subprocess.CalledProcessError(
                65, 'stapler',
                'CloudKit query for /tmp/file.dmg failed due to "(null)"'),
            subprocess.CalledProcessError(999, 'stapler',
                                          'An unexpected error.'),
        ]
        with self.assertRaises(subprocess.CalledProcessError) as cm:
            notarize.staple('/tmp/file.dmg')

        self.assertEqual(cm.exception.returncode, 999)
        run_command.assert_has_calls(2 * [
            mock.call(
                ['xcrun', 'stapler', 'staple', '--verbose', '/tmp/file.dmg'])
        ])
        self.assertEqual(1, kwargs['sleep'].call_count)

    @mock.patch.multiple('time', **{'sleep': mock.DEFAULT})
    @mock.patch('signing.commands.run_command')
    def test_fail_three_times(self, run_command, **kwargs):
        run_command.side_effect = [
            subprocess.CalledProcessError(
                65, 'stapler',
                'CloudKit query for /tmp/file.dmg failed due to "(null)"'),
            subprocess.CalledProcessError(
                65, 'stapler',
                'CloudKit query for /tmp/file.dmg failed due to "(null)"'),
            subprocess.CalledProcessError(
                68, 'stapler',
                'A server with the specified hostname could not be found.'),
        ]
        with self.assertRaises(subprocess.CalledProcessError) as cm:
            notarize.staple('/tmp/file.dmg')

        self.assertEqual(cm.exception.returncode, 68)
        run_command.assert_has_calls(3 * [
            mock.call(
                ['xcrun', 'stapler', 'staple', '--verbose', '/tmp/file.dmg'])
        ])
        self.assertEqual(2, kwargs['sleep'].call_count)


class TestRetry(unittest.TestCase):

    def test_retry(self):
        retry = notarize.Retry('test')
        self.assertTrue(retry.failed_should_retry())
        self.assertTrue(retry.keep_going())
        self.assertTrue(retry.failed_should_retry())
        self.assertTrue(retry.keep_going())
        self.assertFalse(retry.failed_should_retry())
        with self.assertRaises(RuntimeError):
            retry.keep_going()

    @mock.patch('time.sleep')
    def test_with_sleep(self, sleep):
        retry = notarize.Retry('test', sleep_before_retry=True)
        self.assertTrue(retry.failed_should_retry())
        self.assertEqual(1, sleep.call_count)
