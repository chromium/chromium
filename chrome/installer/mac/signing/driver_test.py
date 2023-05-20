# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock

from . import driver, model, test_config


def _config_factory():
    return test_config.TestConfig


@mock.patch('signing.config_factory.get_class', _config_factory)
@mock.patch('signing.pipeline.sign_all')
@mock.patch('signing.commands.run_command')
class TestDriverExecution(unittest.TestCase):

    @mock.patch('signing.commands.file_exists')
    def test_main_flow(self, *args):
        manager = mock.Mock()
        for arg in args:
            manager.attach_mock(arg, arg._mock_name)
        driver.main(
            ['--input', '/input', '--output', '/output', '--identity', '-'])

        manager.assert_has_calls([
            mock.call.file_exists('/output'),
            mock.call.file_exists().__bool__(),
            mock.call.run_command(['sw_vers']),
            mock.call.run_command(['xcodebuild', '-version']),
            mock.call.run_command(['xcrun', '-show-sdk-path']),
            mock.call.sign_all(
                mock.ANY,
                mock.ANY,
                disable_packaging=mock.ANY,
                skip_brands=mock.ANY,
                channels=mock.ANY),
        ])

    @mock.patch.multiple(
        'signing.commands', file_exists=mock.DEFAULT, make_dir=mock.DEFAULT)
    def test_make_output_dir(self, *args, **kwargs):
        kwargs['file_exists'].return_value = False
        driver.main(
            ['--input', '/input', '--output', '/output', '--identity', '-'])

        self.assertEqual(1, kwargs['file_exists'].call_count)
        kwargs['file_exists'].assert_called_with('/output')

        self.assertEqual(1, kwargs['make_dir'].call_count)
        kwargs['make_dir'].assert_called_with('/output')

    @mock.patch.multiple(
        'signing.commands', file_exists=mock.DEFAULT, make_dir=mock.DEFAULT)
    def test_existing_output_dir(self, *args, **kwargs):
        kwargs['file_exists'].return_value = True
        driver.main(
            ['--input', '/input', '--output', '/output', '--identity', '-'])

        self.assertEqual(1, kwargs['file_exists'].call_count)
        kwargs['file_exists'].assert_called_with('/output')

        self.assertEqual(0, kwargs['make_dir'].call_count)


@mock.patch('signing.commands.file_exists', lambda s: True)
@mock.patch('signing.config_factory.get_class', _config_factory)
@mock.patch.multiple('signing.driver', _show_tool_versions=mock.DEFAULT)
@mock.patch('signing.pipeline.sign_all')
class TestCommandLine(unittest.TestCase):

    def test_required_arguments(self, sign_all, **kwargs):
        with self.assertRaises(SystemExit):
            driver.main([])
        with self.assertRaises(SystemExit):
            driver.main(['--input', '/in'])
        with self.assertRaises(SystemExit):
            driver.main(['--output', '/out'])
        with self.assertRaises(SystemExit):
            driver.main(['--input', '/in', '--output', '/out'])

    def test_simple_invoke(self, sign_all, **kwargs):
        driver.main(
            ['--input', '/input', '--output', '/output', '--identity', '-'])
        self.assertEquals(1, sign_all.call_count)

        paths = sign_all.call_args.args[0]
        self.assertEquals('/input', paths.input)
        self.assertEquals('/output', paths.output)

        config = sign_all.call_args.args[1]
        self.assertEquals('-', config.identity)
        self.assertEquals(None, config.installer_identity)
        self.assertTrue(config.run_spctl_assess)
        self.assertFalse(config.inject_get_task_allow_entitlement)
        self.assertFalse(sign_all.call_args.kwargs['disable_packaging'])
        self.assertEquals(model.NotarizeAndStapleLevel.NONE, config.notarize)
        self.assertEquals([], sign_all.call_args.kwargs['skip_brands'])
        self.assertEquals([], sign_all.call_args.kwargs['channels'])

    def test_development(self, sign_all, **kwargs):
        driver.main([
            '--input', '/in', '--output', '/out', '--identity', 'Dev',
            '--development'
        ])
        self.assertEquals(1, sign_all.call_count)

        config = sign_all.call_args.args[1]
        self.assertFalse(config.run_spctl_assess)
        self.assertTrue(config.inject_get_task_allow_entitlement)
        self.assertFalse(sign_all.call_args.kwargs['disable_packaging'])

    def test_config_arguments(self, sign_all, **kwargs):
        driver.main([
            '--input', '/i', '--output', '/o', '--identity',
            'Codesign-Identity', '--installer-identity', 'Installer-Identity'
        ])
        self.assertEquals(1, sign_all.call_count)
        config = sign_all.call_args.args[1]
        self.assertEquals('Codesign-Identity', config.identity)
        self.assertEquals('Installer-Identity', config.installer_identity)

    def test_skip_brands(self, sign_all, **kwargs):
        driver.main([
            '--input', '/input', '--output', '/output', '--identity', '-',
            '--skip-brand', 'BRAND1', '--skip-brand', 'BRAND2'
        ])
        self.assertEquals(1, sign_all.call_count)
        self.assertEquals(['BRAND1', 'BRAND2'],
                          sign_all.call_args.kwargs['skip_brands'])

    def test_channels(self, sign_all, **kwargs):
        driver.main([
            '--input', '/input', '--output', '/output', '--identity', '-',
            '--channel', 'dev', '--channel', 'canary'
        ])
        self.assertEquals(1, sign_all.call_count)
        self.assertEquals(['dev', 'canary'],
                          sign_all.call_args.kwargs['channels'])

    def test_disable_packaging(self, sign_all, **kwargs):
        driver.main([
            '--input',
            '/input',
            '--output',
            '/output',
            '--identity',
            '-',
            '--disable-packaging',
        ])
        self.assertEquals(1, sign_all.call_count)
        self.assertTrue(sign_all.call_args.kwargs['disable_packaging'])

    def test_notarize_unspecified(self, sign_all, **kwargs):
        driver.main([
            '--input', '/input', '--output', '/output', '--identity', 'G',
            '--notarize', '--notary-user', 'Notary-User', '--notary-password',
            '@env:NOTARY'
        ])
        self.assertEquals(1, sign_all.call_count)
        config = sign_all.call_args.args[1]
        self.assertEquals(model.NotarizeAndStapleLevel.STAPLE, config.notarize)
        self.assertEquals('Notary-User', config.notary_user)
        self.assertEquals('@env:NOTARY', config.notary_password)

    def test_notarize_specific(self, sign_all, **kwargs):
        driver.main([
            '--input', '/input', '--output', '/output', '--identity', 'G',
            '--notarize', 'nowait', '--notary-user', 'Notary-User',
            '--notary-password', '@env:NOTARY'
        ])
        self.assertEquals(1, sign_all.call_count)
        config = sign_all.call_args.args[1]
        self.assertEquals(model.NotarizeAndStapleLevel.NOWAIT, config.notarize)
        self.assertEquals('Notary-User', config.notary_user)
        self.assertEquals('@env:NOTARY', config.notary_password)

    def test_notarize_missing_args(self, sign_all, **kwargs):
        with self.assertRaises(SystemExit):
            driver.main([
                '--input', '/input', '--output', '/output', '--identity', 'G',
                '--notarize'
            ])
        with self.assertRaises(SystemExit):
            driver.main([
                '--input', '/input', '--output', '/output', '--identity', 'G',
                '--notarize', '--notary-user', 'u'
            ])
        with self.assertRaises(SystemExit):
            driver.main([
                '--input', '/input', '--output', '/output', '--identity', 'G',
                '--notarize', '--notary-password', 'p'
            ])
        with self.assertRaises(SystemExit):
            driver.main([
                '--input', '/input', '--output', '/output', '--identity', 'G',
                '--notarize', '--notary-user', 'u', '--notary-password', 'p',
                '--notarization-tool', 'notarytool'
            ])

    def test_notarize_notarytool(self, sign_all, **kwargs):
        driver.main([
            '--input', '/input', '--output', '/output', '--identity', 'G',
            '--notarization-tool', 'notarytool', '--notarize', 'staple',
            '--notary-user', 'Notary-User', '--notary-password', '@env:NOTARY',
            '--notary-team-id', 'Team1'
        ])
        self.assertEquals(1, sign_all.call_count)
        config = sign_all.call_args.args[1]
        self.assertEquals(model.NotarizeAndStapleLevel.STAPLE, config.notarize)
        self.assertEquals('Notary-User', config.notary_user)
        self.assertEquals('@env:NOTARY', config.notary_password)
        self.assertEquals('Team1', config.notary_team_id)
        self.assertEquals(model.NotarizationTool.NOTARYTOOL,
                          config.notarization_tool)
