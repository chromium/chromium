# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import plistlib
import subprocess
import unittest

from . import notarize, test_common, test_config
from .model import CodeSignedProduct, Paths

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

    @mock.patch('signing.commands.run_command_output')
    def test_fail_once_then_succeed(self, run_command_output):
        run_command_output.side_effect = [
            subprocess.CalledProcessError(
                176, 'altool',
                'Unable to find requested file(s): metadata.xml (1057)'),
            _make_plist({
                'notarization-upload': {
                    'RequestUUID': '600b24b7-8fa2-4fdb-adf9-dff1f8b7858e'
                }
            })
        ]

        config = test_config.TestConfig()
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

        config = test_config.TestConfig()

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

        config = test_config.TestConfig()

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
            239, 'altool', _make_plist({'product-errors': [{
                'code': 9595
            }]}))

        with self.assertRaises(subprocess.CalledProcessError):
            uuids = ['77c0ad17-479e-4b82-946a-73739cf6ca16']
            list(notarize.wait_for_results(uuids, test_config.TestConfig()))

    @mock.patch.multiple('time', **{'sleep': mock.DEFAULT})
    @mock.patch('signing.commands.run_command_output')
    def test_lost_connection_notarization_info(self, run_command_output,
                                               **kwargs):
        run_command_output.side_effect = [
            subprocess.CalledProcessError(
                13, 'altool', '*** Error: Connection failed! Error Message'
                '- The network connection was lost.'),
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

    @mock.patch.multiple('time', **{'sleep': mock.DEFAULT})
    @mock.patch.multiple('signing.commands',
                         **{'run_command_output': mock.DEFAULT})
    def test_timeout(self, **kwargs):
        kwargs['run_command_output'].return_value = _make_plist(
            {'notarization-info': {
                'Status': 'in progress'
            }})
        uuid = '0c652bb4-7d44-4904-8c59-1ee86a376ece'
        uuids = [uuid]
        with self.assertRaises(notarize.NotarizationError) as cm:
            list(notarize.wait_for_results(uuids, test_config.TestConfig()))

        # Python 2 and 3 stringify set() differently.
        self.assertIn(
            str(cm.exception), [
                "Timed out waiting for notarization requests: set(['0c652bb4-7d44-4904-8c59-1ee86a376ece'])",
                "Timed out waiting for notarization requests: {'0c652bb4-7d44-4904-8c59-1ee86a376ece'}"
            ])

        for call in kwargs['run_command_output'].mock_calls:
            self.assertEqual(
                call,
                mock.call([
                    'xcrun', 'altool', '--notarization-info', uuid,
                    '--username', '[NOTARY-USER]', '--password',
                    '[NOTARY-PASSWORD]', '--output-format', 'xml'
                ]))

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
