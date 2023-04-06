# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import plistlib
import subprocess
import unittest
from unittest import mock

from . import notarize, test_config
from .model import CodeSignedProduct, NotarizationTool, Paths


@mock.patch.multiple(
    'signing.notarize', **{
        m: mock.DEFAULT
        for m in ('_submit_altool', '_submit_notarytool', '_get_result_altool',
                  '_get_result_notarytool', '_get_log_notarytool')
    })
class TestConfigurableNotarizationTool(unittest.TestCase):

    def test_altool_submit(self, **kwargs):
        uuid = '03e0fb6e-4e80-4b7e-833c-c38ecf3f6efd'
        kwargs['_submit_altool'].return_value = uuid
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.ALTOOL)
        self.assertEqual(uuid, notarize.submit('/path/to/app.zip', config))
        kwargs['_submit_altool'].assert_called_with('/path/to/app.zip', config)
        kwargs['_submit_notarytool'].assert_not_called()

    def test_notarytool_submit(self, **kwargs):
        uuid = '03e0fb6e-4e80-4b7e-833c-c38ecf3f6efd'
        kwargs['_submit_notarytool'].return_value = uuid
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.NOTARYTOOL)
        self.assertEqual(uuid, notarize.submit('/path/to/app.zip', config))
        kwargs['_submit_notarytool'].assert_called_with('/path/to/app.zip',
                                                        config)
        kwargs['_submit_altool'].assert_not_called()

    def test_altool_wait_for_results(self, **kwargs):
        uuid = '73b579b0-af5b-46c8-b855-ea6b4d6a926b'
        kwargs['_get_result_altool'].return_value = notarize.NotarizationResult(
            notarize.Status.ERROR, 'Failed', 'Some silly error',
            'https://logs.example.com/notarize.json')
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.ALTOOL)
        with self.assertRaises(notarize.NotarizationError) as cm:
            list(notarize.wait_for_results([uuid], config))
        kwargs['_get_result_altool'].assert_called_with(uuid, config)
        kwargs['_get_result_notarytool'].assert_not_called()

    def test_notarytool_wait_for_results(self, **kwargs):
        uuid = '73b579b0-af5b-46c8-b855-ea6b4d6a926b'
        log_contents = 'The log file contents'

        kwargs['_get_log_notarytool'].return_value = log_contents
        kwargs[
            '_get_result_notarytool'].return_value = notarize.NotarizationResult(
                notarize.Status.ERROR, 'Failed', 'Some silly error',
                'The log file contents.')
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.NOTARYTOOL)
        with self.assertRaises(notarize.NotarizationError) as cm:
            list(notarize.wait_for_results([uuid], config))
        kwargs['_get_result_notarytool'].assert_called_with(uuid, config)
        kwargs['_get_result_altool'].assert_not_called()


class TestSubmitAltool(unittest.TestCase):

    @mock.patch('signing.commands.run_command_output')
    def test_valid_upload(self, run_command_output):
        run_command_output.return_value = plistlib.dumps({
            'notarization-upload': {
                'RequestUUID': '0c652bb4-7d44-4904-8c59-1ee86a376ece'
            },
        })
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.ALTOOL)
        uuid = notarize.submit('/tmp/file.dmg', config)

        self.assertEqual('0c652bb4-7d44-4904-8c59-1ee86a376ece', uuid)
        run_command_output.assert_called_once_with([
            'xcrun', 'altool', '--notarize-app', '--file', '/tmp/file.dmg',
            '--primary-bundle-id', 'test.signing.bundle_id', '--username',
            '[NOTARY-USER]', '--password', '[NOTARY-PASSWORD]',
            '--output-format', 'xml'
        ])

    @mock.patch('signing.commands.run_command_output')
    def test_valid_upload_with_asc_provider(self, run_command_output):
        run_command_output.return_value = plistlib.dumps({
            'notarization-upload': {
                'RequestUUID': '746f1537-0613-4e49-a9a0-869f2c9dc8e5'
            },
        })
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.ALTOOL,
            notary_asc_provider='[NOTARY-ASC-PROVIDER]')
        uuid = notarize.submit('/tmp/file.dmg', config)

        self.assertEqual('746f1537-0613-4e49-a9a0-869f2c9dc8e5', uuid)
        run_command_output.assert_called_once_with([
            'xcrun', 'altool', '--notarize-app', '--file', '/tmp/file.dmg',
            '--primary-bundle-id', 'test.signing.bundle_id', '--username',
            '[NOTARY-USER]', '--password', '[NOTARY-PASSWORD]',
            '--output-format', 'xml', '--asc-provider', '[NOTARY-ASC-PROVIDER]'
        ])

    @mock.patch('signing.commands.run_command_output')
    def test_valid_upload_with_tool_path(self, run_command_output):
        run_command_output.return_value = plistlib.dumps({
            'notarization-upload': {
                'RequestUUID': '42541f28-b8bf-475e-b153-46a6be2b8cc7'
            },
        })
        config = test_config.TestConfigNotarizationToolOverride(
            notarization_tool=NotarizationTool.ALTOOL)
        uuid = notarize.submit('/tmp/file.dmg', config)

        self.assertEqual('42541f28-b8bf-475e-b153-46a6be2b8cc7', uuid)
        run_command_output.assert_called_once_with([
            '/fun/bin/altool.custom', '--notarize-app', '--file',
            '/tmp/file.dmg', '--primary-bundle-id', 'test.signing.bundle_id',
            '--username', '[NOTARY-USER]', '--password', '[NOTARY-PASSWORD]',
            '--output-format', 'xml'
        ])

    @mock.patch('signing.commands.run_command_output')
    def test_fail_once_then_succeed(self, run_command_output):
        run_command_output.side_effect = [
            subprocess.CalledProcessError(
                176, 'altool',
                'Unable to find requested file(s): metadata.xml (1057)'),
            plistlib.dumps({
                'notarization-upload': {
                    'RequestUUID': '600b24b7-8fa2-4fdb-adf9-dff1f8b7858e'
                }
            })
        ]

        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.ALTOOL)
        uuid = notarize.submit('/tmp/app.zip', config)

        self.assertEqual('600b24b7-8fa2-4fdb-adf9-dff1f8b7858e', uuid)
        run_command_output.assert_has_calls(2 * [
            mock.call([
                'xcrun', 'altool', '--notarize-app', '--file', '/tmp/app.zip',
                '--primary-bundle-id', 'test.signing.bundle_id', '--username',
                '[NOTARY-USER]', '--password', '[NOTARY-PASSWORD]',
                '--output-format', 'xml'
            ])
        ])

    @mock.patch('signing.commands.run_command_output')
    def test_fail_twice_with_unexpected_code(self, run_command_output):
        run_command_output.side_effect = [
            subprocess.CalledProcessError(
                176, 'altool',
                'Unable to find requested file(s): metadata.xml (1057)'),
            subprocess.CalledProcessError(999, 'altool', 'Unexpected error'),
        ]

        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.ALTOOL)

        with self.assertRaises(subprocess.CalledProcessError) as cm:
            notarize.submit('/tmp/app.zip', config)

        self.assertEqual(cm.exception.returncode, 999)
        run_command_output.assert_has_calls(2 * [
            mock.call([
                'xcrun', 'altool', '--notarize-app', '--file', '/tmp/app.zip',
                '--primary-bundle-id', 'test.signing.bundle_id', '--username',
                '[NOTARY-USER]', '--password', '[NOTARY-PASSWORD]',
                '--output-format', 'xml'
            ])
        ])

    @mock.patch('signing.commands.run_command_output')
    def test_fail_three_times(self, run_command_output):
        run_command_output.side_effect = [
            subprocess.CalledProcessError(
                176, 'altool',
                'Unable to find requested file(s): metadata.xml (1057)'),
            subprocess.CalledProcessError(
                236, 'altool',
                'Exception occurred when creating MZContentProviderUpload for provider. (1004)'
            ),
            subprocess.CalledProcessError(
                240, 'altool',
                'A fatal error has been detected by the Java Runtime Environment'
            ),
        ]

        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.ALTOOL)

        with self.assertRaises(subprocess.CalledProcessError) as cm:
            notarize.submit('/tmp/app.zip', config)

        self.assertEqual(cm.exception.returncode, 240)
        run_command_output.assert_has_calls(3 * [
            mock.call([
                'xcrun', 'altool', '--notarize-app', '--file', '/tmp/app.zip',
                '--primary-bundle-id', 'test.signing.bundle_id', '--username',
                '[NOTARY-USER]', '--password', '[NOTARY-PASSWORD]',
                '--output-format', 'xml'
            ])
        ])


class TestSubmitNotarytool(unittest.TestCase):

    @mock.patch('signing.commands.run_password_command_output')
    def test_valid_upload(self, run_password_command_output):
        run_password_command_output.return_value = plistlib.dumps({
            'id': '13d6aa9b-d204-4f0d-9164-4bda5e730258',
            'message': 'Successfully uploaded file',
            'path': '/tmp/file.dmg'
        })
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.NOTARYTOOL)
        uuid = notarize.submit('/tmp/file.dmg', config)

        self.assertEqual('13d6aa9b-d204-4f0d-9164-4bda5e730258', uuid)
        run_password_command_output.assert_called_once_with([
            'xcrun', 'notarytool', 'submit', '/tmp/file.dmg', '--apple-id',
            '[NOTARY-USER]', '--team-id', '[NOTARY-TEAM]', '--no-wait',
            '--output-format', 'plist'
        ], '[NOTARY-PASSWORD]')

    @mock.patch('signing.commands.run_password_command_output')
    def test_valid_upload_with_env_password(self, run_password_command_output):
        os.environ['NOTARIZE_TEST_PASSWORD'] = 'hunter2'
        run_password_command_output.return_value = plistlib.dumps(
            {'id': 'b53b3ed1-82cb-41b4-9e12-b097b2c05f64'})
        config = test_config.TestConfig(
            notary_password='@env:NOTARIZE_TEST_PASSWORD',
            notarization_tool=NotarizationTool.NOTARYTOOL)
        uuid = notarize.submit('/tmp/file.dmg', config)
        del os.environ['NOTARIZE_TEST_PASSWORD']

        self.assertEqual('b53b3ed1-82cb-41b4-9e12-b097b2c05f64', uuid)
        run_password_command_output.assert_called_once_with([
            'xcrun', 'notarytool', 'submit', '/tmp/file.dmg', '--apple-id',
            '[NOTARY-USER]', '--team-id', '[NOTARY-TEAM]', '--no-wait',
            '--output-format', 'plist'
        ], 'hunter2')

    @mock.patch('signing.commands.run_password_command_output')
    def test_valid_upload_with_tool_path(self, run_password_command_output):
        run_password_command_output.return_value = plistlib.dumps({
            'id': '44346a58-41f9-47c4-b63a-f3831732a553',
            'message': 'Successfully uploaded file',
            'path': '/tmp/file.dmg'
        })
        config = test_config.TestConfigNotarizationToolOverride(
            notarization_tool=NotarizationTool.NOTARYTOOL)
        uuid = notarize.submit('/tmp/file.dmg', config)

        self.assertEqual('44346a58-41f9-47c4-b63a-f3831732a553', uuid)
        run_password_command_output.assert_called_once_with([
            '/fun/bin/notarytool.custom', 'submit', '/tmp/file.dmg',
            '--apple-id', '[NOTARY-USER]', '--team-id', '[NOTARY-TEAM]',
            '--no-wait', '--output-format', 'plist'
        ], '[NOTARY-PASSWORD]')


class TestGetResultAltool(unittest.TestCase):

    @mock.patch('signing.commands.run_command_output')
    def test_success(self, run_command_output):
        plist_output = plistlib.dumps({
            'notarization-info': {
                'Date': '2019-05-20T13:18:35Z',
                'LogFileURL': 'https://example.com/log.json',
                'RequestUUID': 'cca0aec2-7c64-4ea4-b895-051ea3a17311',
                'Status': 'success',
                'Status Code': 0
            }
        })
        run_command_output.return_value = plist_output
        uuid = 'cca0aec2-7c64-4ea4-b895-051ea3a17311'
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.ALTOOL)
        result = notarize._get_result_altool(uuid, config)
        self.assertEqual(notarize.Status.SUCCESS, result.status)
        self.assertEqual('success', result.status_string)
        self.assertEqual(plist_output, result.output)
        self.assertEqual('https://example.com/log.json', result.log_file)
        run_command_output.assert_called_once_with([
            'xcrun', 'altool', '--notarization-info', uuid, '--username',
            '[NOTARY-USER]', '--password', '[NOTARY-PASSWORD]',
            '--output-format', 'xml'
        ])

    @mock.patch('signing.commands.run_command_output')
    def test_success_with_asc_provider(self, run_command_output):
        run_command_output.return_value = plistlib.dumps({
            'notarization-info': {
                'Date': '2019-07-08T20:11:24Z',
                'LogFileURL': 'https://example.com/log.json',
                'RequestUUID': '0a88b2d8-4098-4d3a-8461-5b543b479d15',
                'Status': 'success',
                'Status Code': 0
            }
        })
        uuid = '0a88b2d8-4098-4d3a-8461-5b543b479d15'
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.ALTOOL,
            notary_asc_provider='[NOTARY-ASC-PROVIDER]')
        self.assertEqual(notarize.Status.SUCCESS,
                         notarize._get_result_altool(uuid, config).status)
        run_command_output.assert_called_once_with([
            'xcrun', 'altool', '--notarization-info', uuid, '--username',
            '[NOTARY-USER]', '--password', '[NOTARY-PASSWORD]',
            '--output-format', 'xml', '--asc-provider', '[NOTARY-ASC-PROVIDER]'
        ])

    @mock.patch('signing.commands.run_command_output')
    def test_success_with_tool_path(self, run_command_output):
        plist_output = plistlib.dumps({
            'notarization-info': {
                'Date': '2019-05-20T13:18:35Z',
                'LogFileURL': 'https://example.com/log.json',
                'RequestUUID': '4711f98f-d509-43a4-a3da-537e7e885159',
                'Status': 'success',
                'Status Code': 0
            }
        })
        run_command_output.return_value = plist_output
        uuid = '4711f98f-d509-43a4-a3da-537e7e885159'
        config = test_config.TestConfigNotarizationToolOverride(
            notarization_tool=NotarizationTool.ALTOOL)
        result = notarize._get_result_altool(uuid, config)
        self.assertEqual(notarize.Status.SUCCESS, result.status)
        self.assertEqual('success', result.status_string)
        self.assertEqual(plist_output, result.output)
        self.assertEqual('https://example.com/log.json', result.log_file)
        run_command_output.assert_called_once_with([
            '/fun/bin/altool.custom', '--notarization-info', uuid, '--username',
            '[NOTARY-USER]', '--password', '[NOTARY-PASSWORD]',
            '--output-format', 'xml'
        ])

    @mock.patch('signing.commands.run_command_output')
    def test_failure(self, run_command_output):
        plist_output = plistlib.dumps({
            'notarization-info': {
                'Date': '2019-05-20T13:18:35Z',
                'LogFileURL': 'https://example.com/log.json',
                'RequestUUID': 'cca0aec2-7c64-4ea4-b895-051ea3a17311',
                'Status': 'invalid',
                'Status Code': 2,
                'Status Message': 'Package Invalid',
            }
        })
        run_command_output.return_value = plist_output
        uuid = 'cca0aec2-7c64-4ea4-b895-051ea3a17311'
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.ALTOOL)
        result = notarize._get_result_altool(uuid, config)
        self.assertEqual(notarize.Status.ERROR, result.status)
        self.assertEqual('invalid', result.status_string)
        self.assertEqual(plist_output, result.output)
        self.assertEqual('https://example.com/log.json', result.log_file)

    @mock.patch.multiple('time', **{'sleep': mock.DEFAULT})
    @mock.patch('signing.commands.run_command_output')
    def test_fresh_request_race(self, run_command_output, **kwargs):
        run_command_output.side_effect = [
            subprocess.CalledProcessError(
                239, 'altool',
                plistlib.dumps({
                    'product-errors': [{
                        'code': 1519,
                        'message': 'Could not find the RequestUUID.',
                        'userInfo': {
                            'NSLocalizedDescription':
                                'Could not find the RequestUUID.',
                            'NSLocalizedFailureReason':
                                'Apple Services operation failed.',
                            'NSLocalizedRecoverySuggestion':
                                'Could not find the RequestUUID.'
                        }
                    }]
                })),
            plistlib.dumps({
                'notarization-info': {
                    'Date': '2019-05-20T13:18:35Z',
                    'LogFileURL': 'https://example.com/log.json',
                    'RequestUUID': 'cca0aec2-7c64-4ea4-b895-051ea3a17311',
                    'Status': 'success',
                    'Status Code': 0
                }
            })
        ]
        uuid = 'cca0aec2-7c64-4ea4-b895-051ea3a17311'
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.ALTOOL)
        self.assertEqual(notarize.Status.IN_PROGRESS,
                         notarize._get_result_altool(uuid, config).status)
        self.assertEqual(notarize.Status.SUCCESS,
                         notarize._get_result_altool(uuid, config).status)
        run_command_output.assert_has_calls(2 * [
            mock.call([
                'xcrun', 'altool', '--notarization-info', uuid, '--username',
                '[NOTARY-USER]', '--password', '[NOTARY-PASSWORD]',
                '--output-format', 'xml'
            ])
        ])

    @mock.patch('signing.commands.run_command_output')
    def test_bad_notarization_info(self, run_command_output, **kwargs):
        run_command_output.side_effect = subprocess.CalledProcessError(
            239, 'altool', plistlib.dumps({'product-errors': [{
                'code': 9595
            }]}))

        with self.assertRaises(subprocess.CalledProcessError):
            notarize._get_result_altool(
                '77c0ad17-479e-4b82-946a-73739cf6ca16',
                test_config.TestConfig(
                    notarization_tool=NotarizationTool.ALTOOL))

    @mock.patch.multiple('time', **{'sleep': mock.DEFAULT})
    @mock.patch('signing.commands.run_command_output')
    def test_lost_connection_notarization_info(self, run_command_output,
                                               **kwargs):
        run_command_output.side_effect = [
            subprocess.CalledProcessError(
                13, 'altool', '*** Error: Connection failed! Error Message'
                '- The network connection was lost.'),
            plistlib.dumps({
                'notarization-info': {
                    'Date': '2019-05-20T13:18:35Z',
                    'LogFileURL': 'https://example.com/log.json',
                    'RequestUUID': 'cca0aec2-7c64-4ea4-b895-051ea3a17311',
                    'Status': 'success',
                    'Status Code': 0
                }
            })
        ]
        uuid = 'cca0aec2-7c64-4ea4-b895-051ea3a17311'
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.ALTOOL)
        self.assertEqual(notarize.Status.IN_PROGRESS,
                         notarize._get_result_altool(uuid, config).status)
        self.assertEqual(notarize.Status.SUCCESS,
                         notarize._get_result_altool(uuid, config).status)
        run_command_output.assert_has_calls(2 * [
            mock.call([
                'xcrun', 'altool', '--notarization-info', uuid, '--username',
                '[NOTARY-USER]', '--password', '[NOTARY-PASSWORD]',
                '--output-format', 'xml'
            ])
        ])

    @mock.patch.multiple('time', **{'sleep': mock.DEFAULT})
    @mock.patch('signing.commands.run_command_output')
    def test_notarization_info_exit_1(self, run_command_output, **kwargs):
        run_command_output.side_effect = [
            subprocess.CalledProcessError(1, 'altool', ''),
            plistlib.dumps({
                'notarization-info': {
                    'Date': '2021-08-24T19:28:21Z',
                    'LogFileURL': 'https://example.com/log.json',
                    'RequestUUID': 'a11980d4-24ef-4040-bddd-f8341859fb6e',
                    'Status': 'success',
                    'Status Code': 0
                }
            })
        ]
        uuid = 'a11980d4-24ef-4040-bddd-f8341859fb6e'
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.ALTOOL)
        self.assertEqual(notarize.Status.IN_PROGRESS,
                         notarize._get_result_altool(uuid, config).status)
        self.assertEqual(notarize.Status.SUCCESS,
                         notarize._get_result_altool(uuid, config).status)
        run_command_output.assert_has_calls(2 * [
            mock.call([
                'xcrun', 'altool', '--notarization-info', uuid, '--username',
                '[NOTARY-USER]', '--password', '[NOTARY-PASSWORD]',
                '--output-format', 'xml'
            ])
        ])


class TestGetResultNotarytool(unittest.TestCase):

    @mock.patch('signing.commands.run_password_command_output')
    def test_successs(self, run_password_command_output):
        plist_output = plistlib.dumps({
            'status': 'Accepted',
            'id': 'eeeacc17-9e4b-4408-8001-894bbae9c9e9',
            'createdDate': '2022-03-10T23:04:21.192Z',
            'message': 'Successfully received submission info',
            'name': 'file.zip'
        })
        run_password_command_output.return_value = plist_output
        uuid = 'eeeacc17-9e4b-4408-8001-894bbae9c9e9'
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.NOTARYTOOL)
        result = notarize._get_result_notarytool(uuid, config)

        self.assertEqual(notarize.Status.SUCCESS, result.status)
        self.assertEqual('Accepted', result.status_string)
        self.assertEqual(plist_output, result.output)
        self.assertEqual(None, result.log_file)

        run_password_command_output.assert_called_once_with([
            'xcrun', 'notarytool', 'info', uuid, '--apple-id', '[NOTARY-USER]',
            '--team-id', '[NOTARY-TEAM]', '--output-format', 'plist'
        ], '[NOTARY-PASSWORD]')

    @mock.patch('signing.commands.run_password_command_output')
    def test_success_with_env_password(self, run_password_command_output):
        plist_output = plistlib.dumps({
            'status': 'Accepted',
            'id': 'eeeacc17-9e4b-4408-8001-894bbae9c9e9',
            'createdDate': '2022-03-10T23:04:21.192Z',
            'message': 'Successfully received submission info',
            'name': 'file.zip'
        })
        run_password_command_output.return_value = plist_output
        uuid = 'eeeacc17-9e4b-4408-8001-894bbae9c9e9'
        os.environ['NOTARIZE_TEST_PASSWORD'] = 'hunter2'
        config = test_config.TestConfig(
            notary_password='@env:NOTARIZE_TEST_PASSWORD',
            notarization_tool=NotarizationTool.NOTARYTOOL)
        result = notarize._get_result_notarytool(uuid, config)
        del os.environ['NOTARIZE_TEST_PASSWORD']

        self.assertEqual(notarize.Status.SUCCESS, result.status)
        self.assertEqual('Accepted', result.status_string)
        self.assertEqual(plist_output, result.output)
        self.assertEqual(None, result.log_file)

        run_password_command_output.assert_called_once_with([
            'xcrun', 'notarytool', 'info', uuid, '--apple-id', '[NOTARY-USER]',
            '--team-id', '[NOTARY-TEAM]', '--output-format', 'plist'
        ], 'hunter2')

    @mock.patch('signing.commands.run_password_command_output')
    def test_successs_with_tool_path(self, run_password_command_output):
        plist_output = plistlib.dumps({
            'status': 'Accepted',
            'id': 'a3b64713-289d-4c57-902a-7fb8270665ef',
            'createdDate': '2022-03-10T23:04:21.192Z',
            'message': 'Successfully received submission info',
            'name': 'file.zip'
        })
        run_password_command_output.return_value = plist_output
        uuid = 'a3b64713-289d-4c57-902a-7fb8270665ef'
        config = test_config.TestConfigNotarizationToolOverride(
            notarization_tool=NotarizationTool.NOTARYTOOL)
        result = notarize._get_result_notarytool(uuid, config)

        self.assertEqual(notarize.Status.SUCCESS, result.status)
        self.assertEqual('Accepted', result.status_string)
        self.assertEqual(plist_output, result.output)
        self.assertEqual(None, result.log_file)

        run_password_command_output.assert_called_once_with([
            '/fun/bin/notarytool.custom', 'info', uuid, '--apple-id',
            '[NOTARY-USER]', '--team-id', '[NOTARY-TEAM]', '--output-format',
            'plist'
        ], '[NOTARY-PASSWORD]')

    @mock.patch('signing.commands.run_password_command_output')
    def test_in_progress(self, run_password_command_output):
        plist_output = plistlib.dumps({
            'status': 'In Progress',
            'id': 'eeeacc17-9e4b-4408-8001-894bbae9c9e9',
            'createdDate': '2022-03-10T23:04:21.192Z',
            'message': 'Successfully received submission info',
            'name': 'file.zip'
        })
        run_password_command_output.return_value = plist_output
        uuid = 'eeeacc17-9e4b-4408-8001-894bbae9c9e9'
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.NOTARYTOOL)
        result = notarize._get_result_notarytool(uuid, config)

        self.assertEqual(notarize.Status.IN_PROGRESS, result.status)
        self.assertEqual('In Progress', result.status_string)
        self.assertEqual(plist_output, result.output)
        self.assertEqual(None, result.log_file)

        run_password_command_output.assert_called_once_with([
            'xcrun', 'notarytool', 'info', uuid, '--apple-id', '[NOTARY-USER]',
            '--team-id', '[NOTARY-TEAM]', '--output-format', 'plist'
        ], '[NOTARY-PASSWORD]')

    @mock.patch('signing.commands.run_password_command_output')
    def test_rejected(self, run_password_command_output):
        plist_output = plistlib.dumps({
            'status': 'Invalid',
            'id': '13d6aa9b-d204-4f0d-9164-4bda5e730258',
            'createdDate': '2022-03-10T21:28:01.741Z',
            'message': 'Successfully received submission info',
            'name': 'chromium_helper.zip'
        })
        run_password_command_output.side_effect = [
            plist_output, b'This is the log file contents'
        ]
        uuid = '13d6aa9b-d204-4f0d-9164-4bda5e730258'
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.NOTARYTOOL)
        result = notarize._get_result_notarytool(uuid, config)

        self.assertEqual(notarize.Status.ERROR, result.status)
        self.assertEqual('Invalid', result.status_string)
        self.assertEqual(plist_output, result.output)
        self.assertEqual('This is the log file contents', result.log_file)

        run_password_command_output.assert_has_calls([
            mock.call([
                'xcrun', 'notarytool', 'info', uuid, '--apple-id',
                '[NOTARY-USER]', '--team-id', '[NOTARY-TEAM]',
                '--output-format', 'plist'
            ], '[NOTARY-PASSWORD]'),
            mock.call([
                'xcrun', 'notarytool', 'log', uuid, '--apple-id',
                '[NOTARY-USER]', '--team-id', '[NOTARY-TEAM]'
            ], '[NOTARY-PASSWORD]')
        ])

    @mock.patch('signing.commands.run_password_command_output')
    def test_rejected_failed_get_log(self, run_password_command_output):
        plist_output = plistlib.dumps({
            'status': 'Invalid',
            'id': '13d6aa9b-d204-4f0d-9164-4bda5e730258',
            'createdDate': '2022-03-10T21:28:01.741Z',
            'message': 'Successfully received submission info',
            'name': 'chromium_helper.zip'
        })
        run_password_command_output.side_effect = [
            plist_output,
            subprocess.CalledProcessError(1, 'notarytool', 'Error message.')
        ]
        uuid = '13d6aa9b-d204-4f0d-9164-4bda5e730258'
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.NOTARYTOOL)
        result = notarize._get_result_notarytool(uuid, config)

        self.assertEqual(notarize.Status.ERROR, result.status)
        self.assertEqual('Invalid', result.status_string)
        self.assertEqual(plist_output, result.output)
        self.assertEqual(None, result.log_file)

        run_password_command_output.assert_has_calls([
            mock.call([
                'xcrun', 'notarytool', 'info', uuid, '--apple-id',
                '[NOTARY-USER]', '--team-id', '[NOTARY-TEAM]',
                '--output-format', 'plist'
            ], '[NOTARY-PASSWORD]'),
            mock.call([
                'xcrun', 'notarytool', 'log', uuid, '--apple-id',
                '[NOTARY-USER]', '--team-id', '[NOTARY-TEAM]'
            ], '[NOTARY-PASSWORD]')
        ])


class TestWaitForResults(unittest.TestCase):

    @mock.patch('signing.notarize._get_result_notarytool')
    def test_success(self, get_result):
        get_result.return_value = notarize.NotarizationResult(
            notarize.Status.SUCCESS, 'success', None,
            'https://example.com/log.json')
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.NOTARYTOOL)
        uuid = 'cca0aec2-7c64-4ea4-b895-051ea3a17311'
        uuids = [uuid]
        self.assertEqual(uuids, list(notarize.wait_for_results(uuids, config)))
        get_result.assert_called_once_with(uuid, config)

    @mock.patch('signing.notarize._get_result_notarytool')
    def test_failure(self, get_result):
        get_result.return_value = notarize.NotarizationResult(
            notarize.Status.ERROR, 'invalid', 'Package Invalid',
            'https://example.com/log.json')
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.NOTARYTOOL)
        uuid = 'cca0aec2-7c64-4ea4-b895-051ea3a17311'
        uuids = [uuid]
        with self.assertRaises(notarize.NotarizationError) as cm:
            list(notarize.wait_for_results(uuids, config))

        self.assertEqual(
            'Notarization request cca0aec2-7c64-4ea4-b895-051ea3a17311 failed '
            'with status: "invalid".', str(cm.exception))

    @mock.patch('signing.notarize._get_result_notarytool')
    def test_subprocess_errors(self, get_result):
        get_result.side_effect = subprocess.CalledProcessError(
            1, 'notarytool', 'A mysterious error occurred.')

        with self.assertRaises(subprocess.CalledProcessError):
            uuids = ['77c0ad17-479e-4b82-946a-73739cf6ca16']
            list(
                notarize.wait_for_results(
                    uuids,
                    test_config.TestConfig(
                        notarization_tool=NotarizationTool.NOTARYTOOL)))

    @mock.patch.multiple('time', **{'sleep': mock.DEFAULT})
    @mock.patch.multiple('signing.notarize',
                         **{'_get_result_notarytool': mock.DEFAULT})
    def test_timeout(self, **kwargs):
        kwargs[
            '_get_result_notarytool'].return_value = notarize.NotarizationResult(
                notarize.Status.IN_PROGRESS, None, None, None)
        config = test_config.TestConfig(
            notarization_tool=NotarizationTool.NOTARYTOOL)
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

        for call in kwargs['_get_result_notarytool'].mock_calls:
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

    @mock.patch('signing.commands.run_command')
    def test_fail_once_then_succeed(self, run_command):
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

    @mock.patch('signing.commands.run_command')
    def test_fail_twice_with_unexpected_code(self, run_command):
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

    @mock.patch('signing.commands.run_command')
    def test_fail_three_times(self, run_command):
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
