# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import plistlib
import subprocess
import unittest

from . import notarize, test_common, test_config

mock = test_common.import_mock()


# python2 support.
def _make_plist(d):
    if hasattr(plistlib, 'dumps'):
        return plistlib.dumps(d)
    else:
        as_str = plistlib.writePlistToString(d)
        return bytes(as_str)


class TestSubmit(unittest.TestCase):

    @mock.patch('signing.commands.run_command_output')
    def test_valid_upload(self, run_command_output):
        run_command_output.return_value = _make_plist({
            'notarization-upload': {
                'RequestUUID': '0c652bb4-7d44-4904-8c59-1ee86a376ece'
            },
        })
        config = test_config.TestConfig()
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
        run_command_output.return_value = _make_plist({
            'notarization-upload': {
                'RequestUUID': '746f1537-0613-4e49-a9a0-869f2c9dc8e5'
            },
        })
        config = test_config.TestConfig(
            notary_asc_provider='[NOTARY-ASC-PROVIDER]')
        uuid = notarize.submit('/tmp/file.dmg', config)

        self.assertEqual('746f1537-0613-4e49-a9a0-869f2c9dc8e5', uuid)
        run_command_output.assert_called_once_with([
            'xcrun', 'altool', '--notarize-app', '--file', '/tmp/file.dmg',
            '--primary-bundle-id', 'test.signing.bundle_id', '--username',
            '[NOTARY-USER]', '--password', '[NOTARY-PASSWORD]',
            '--output-format', 'xml', '--asc-provider', '[NOTARY-ASC-PROVIDER]'
        ])


class TestWaitForResults(unittest.TestCase):

    @mock.patch('signing.commands.run_command_output')
    def test_success(self, run_command_output):
        run_command_output.return_value = _make_plist({
            'notarization-info': {
                'Date': '2019-05-20T13:18:35Z',
                'LogFileURL': 'https://example.com/log.json',
                'RequestUUID': 'cca0aec2-7c64-4ea4-b895-051ea3a17311',
                'Status': 'success',
                'Status Code': 0
            }
        })
        uuid = 'cca0aec2-7c64-4ea4-b895-051ea3a17311'
        uuids = [uuid]
        self.assertEqual(
            uuids,
            list(notarize.wait_for_results(uuids, test_config.TestConfig())))
        run_command_output.assert_called_once_with([
            'xcrun', 'altool', '--notarization-info', uuid, '--username',
            '[NOTARY-USER]', '--password', '[NOTARY-PASSWORD]',
            '--output-format', 'xml'
        ])

    @mock.patch('signing.commands.run_command_output')
    def test_success_with_asc_provider(self, run_command_output):
        run_command_output.return_value = _make_plist({
            'notarization-info': {
                'Date': '2019-07-08T20:11:24Z',
                'LogFileURL': 'https://example.com/log.json',
                'RequestUUID': '0a88b2d8-4098-4d3a-8461-5b543b479d15',
                'Status': 'success',
                'Status Code': 0
            }
        })
        uuid = '0a88b2d8-4098-4d3a-8461-5b543b479d15'
        uuids = [uuid]
        self.assertEqual(
            uuids,
            list(
                notarize.wait_for_results(
                    uuids,
                    test_config.TestConfig(
                        notary_asc_provider='[NOTARY-ASC-PROVIDER]'))))
        run_command_output.assert_called_once_with([
            'xcrun', 'altool', '--notarization-info', uuid, '--username',
            '[NOTARY-USER]', '--password', '[NOTARY-PASSWORD]',
            '--output-format', 'xml', '--asc-provider', '[NOTARY-ASC-PROVIDER]'
        ])

    @mock.patch('signing.commands.run_command_output')
    def test_failure(self, run_command_output):
        run_command_output.return_value = _make_plist({
            'notarization-info': {
                'Date': '2019-05-20T13:18:35Z',
                'LogFileURL': 'https://example.com/log.json',
                'RequestUUID': 'cca0aec2-7c64-4ea4-b895-051ea3a17311',
                'Status': 'invalid',
                'Status Code': 2,
                'Status Message': 'Package Invalid',
            }
        })
        uuid = 'cca0aec2-7c64-4ea4-b895-051ea3a17311'
        uuids = [uuid]
        with self.assertRaises(notarize.NotarizationError) as cm:
            list(notarize.wait_for_results(uuids, test_config.TestConfig()))

        self.assertEqual(
            'Notarization request cca0aec2-7c64-4ea4-b895-051ea3a17311 failed '
            'with status: "invalid". Log file: https://example.com/log.json.',
            str(cm.exception))

    @mock.patch.multiple('time', **{'sleep': mock.DEFAULT})
    @mock.patch('signing.commands.run_command_output')
    def test_fresh_request_race(self, run_command_output, **kwargs):
        run_command_output.side_effect = [
            subprocess.CalledProcessError(
                239, 'altool',
                _make_plist({
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
            _make_plist({
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
        uuids = [uuid]
        self.assertEqual(
            [uuid],
            list(notarize.wait_for_results(uuids, test_config.TestConfig())))
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
            239, 'altool', _make_plist({
                'product-errors': [{
                    'code': 9595
                }]
            }))

        with self.assertRaises(subprocess.CalledProcessError):
            uuids = ['77c0ad17-479e-4b82-946a-73739cf6ca16']
            list(notarize.wait_for_results(uuids, test_config.TestConfig()))

    @mock.patch.multiple('time', **{'sleep': mock.DEFAULT})
    @mock.patch.multiple('signing.commands',
                         **{'run_command_output': mock.DEFAULT})
    def test_timeout(self, **kwargs):
        kwargs['run_command_output'].return_value = _make_plist({
            'notarization-info': {
                'Status': 'in progress'
            }
        })
        uuid = '0c652bb4-7d44-4904-8c59-1ee86a376ece'
        uuids = [uuid]
        with self.assertRaises(notarize.NotarizationError) as cm:
            list(notarize.wait_for_results(uuids, test_config.TestConfig()))

        self.assertEqual(
            "Timed out waiting for notarization requests: ['0c652bb4-7d44-4904-8c59-1ee86a376ece']",
            str(cm.exception))

        for call in kwargs['run_command_output'].mock_calls:
            self.assertEqual(
                call,
                mock.call([
                    'xcrun', 'altool', '--notarization-info', uuid,
                    '--username', '[NOTARY-USER]', '--password',
                    '[NOTARY-PASSWORD]', '--output-format', 'xml'
                ]))

        total_time = sum([call[1][0] for call in kwargs['sleep'].mock_calls])
        self.assertLess(total_time, 31 * 60)


class TestStaple(unittest.TestCase):

    @mock.patch('signing.commands.run_command')
    def test_staple(self, run_command):
        notarize.staple('/tmp/file.dmg')
        run_command.assert_called_once_with(
            ['xcrun', 'stapler', 'staple', '--verbose', '/tmp/file.dmg'])
