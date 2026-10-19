# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import asyncio
import unittest
from unittest import mock

from signing import model, pipeline


def _get_work_dir(*args, **kwargs):
    _get_work_dir.count += 1
    return '/$W_{}'.format(_get_work_dir.count)


_get_work_dir.count = 0


@mock.patch.multiple(
    'signing.commands',
    **{
        m: mock.DEFAULT
        for m in (
            'move_file',
            'copy_files',
            'run_command',
            'make_dir',
            'delete_file_if_exists',
            'shutil',
        )
    },
)
@mock.patch.multiple(
    'signing.signing',
    **{m: mock.DEFAULT for m in ('sign_part', 'verify_part', 'validate_app')},
)
@mock.patch.multiple('signing.parts', **{'sign_all': mock.DEFAULT})
@mock.patch('signing.commands.tempfile.mkdtemp', _get_work_dir)
class TestPipeline(unittest.TestCase):
    def setUp(self):
        _get_work_dir.count = 0
        self.paths = model.Paths('/$I', '/$O', None)

    def test_sign_app(self, **kwargs):
        config = mock.Mock()
        config.app_dir = 'ViewsExamples.app'
        config.app_product = 'ViewsExamples'
        paths = self.paths.replace_work('/$W')

        asyncio.run(pipeline._sign_app(paths, config, '/$O/$D'))

        kwargs['copy_files'].assert_has_calls(
            [
                mock.call('/$I/ViewsExamples.app', '/$W'),
            ]
        )
        kwargs['sign_all'].assert_awaited_once_with(paths, config)
        kwargs['make_dir'].assert_called_once_with('/$O/$D')
        kwargs['move_file'].assert_has_calls(
            [
                mock.call('/$W/ViewsExamples.app', '/$O/$D/ViewsExamples.app'),
            ]
        )

    def test_package_zip(self, **kwargs):
        config = mock.Mock()
        config.packaging_basename = 'ViewsExamples_mac'
        config.app_product = 'ViewsExamples'
        config.app_dir = 'ViewsExamples.app'
        paths = self.paths.replace_work('/$W')

        res = pipeline._package_zip(paths, config)

        self.assertEqual('/$O/ViewsExamples_mac.zip', res)
        # An existing ZIP must be removed rather than updated in place.
        kwargs['delete_file_if_exists'].assert_called_once_with(
            '/$O/ViewsExamples_mac.zip'
        )
        kwargs['run_command'].assert_called_once_with(
            [
                'zip',
                '-9',
                '--recurse-paths',
                '--symlinks',
                '--quiet',
                '/$O/ViewsExamples_mac.zip',
                'ViewsExamples.app',
            ],
            cwd='/$W',
        )

    @mock.patch('signing.notarize.staple_bundled_parts')
    @mock.patch('signing.notarize.submit')
    def test_sign_all(self, submit, staple_bundled_parts, **kwargs):
        config = mock.Mock()
        config.packaging_basename = 'ViewsExamples_mac'
        config.app_product = 'ViewsExamples'
        config.app_dir = 'ViewsExamples.app'
        config.notarize.should_notarize.return_value = False
        config.notarize.should_staple.return_value = False

        orig_paths = self.paths

        asyncio.run(pipeline.sign_all(orig_paths, config))

        # Without notarization, the only ZIP is the one packaged into the
        # output directory, from the signed app in the notary work dir.
        kwargs['run_command'].assert_called_once_with(
            [
                'zip',
                '-9',
                '--recurse-paths',
                '--symlinks',
                '--quiet',
                '/$O/ViewsExamples_mac.zip',
                'ViewsExamples.app',
            ],
            cwd='/$W_1/ViewsExamples_mac',
        )
        self.assertEqual(0, submit.call_count)
        self.assertEqual(0, staple_bundled_parts.call_count)

    @mock.patch('signing.notarize.staple_bundled_parts')
    @mock.patch('signing.notarize.submit')
    def test_sign_all_notarize(self, submit, staple_bundled_parts, **kwargs):
        config = mock.Mock()
        config.packaging_basename = 'ViewsExamples_mac'
        config.app_product = 'ViewsExamples'
        config.app_dir = 'ViewsExamples.app'
        config.notarize.should_notarize.return_value = True
        config.notarize.should_staple.return_value = True

        orig_paths = self.paths

        asyncio.run(pipeline.sign_all(orig_paths, config))

        # Check it submits for notarization and staples
        kwargs['run_command'].assert_any_call(
            [
                'zip',
                '--recurse-paths',
                '--symlinks',
                '--quiet',
                '/$W_1/ViewsExamples_mac.zip',
                'ViewsExamples.app',
            ],
            cwd='/$W_1/ViewsExamples_mac',
        )

        submit.assert_awaited_once_with('/$W_1/ViewsExamples_mac.zip', config)
        staple_bundled_parts.assert_called_once()
        self.assertEqual(
            '/$W_1/ViewsExamples_mac',
            staple_bundled_parts.call_args.args[1].work,
        )

        # The stapled app is then packaged into the output directory.
        self.assertEqual(
            mock.call(
                [
                    'zip',
                    '-9',
                    '--recurse-paths',
                    '--symlinks',
                    '--quiet',
                    '/$O/ViewsExamples_mac.zip',
                    'ViewsExamples.app',
                ],
                cwd='/$W_1/ViewsExamples_mac',
            ),
            kwargs['run_command'].call_args,
        )
