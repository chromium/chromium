# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock

from signing import model, modification, test_config


def plist_read(*args):
    bundle_id = test_config.TestConfig().base_bundle_id
    plists = {
        '/$W/App Product.app/Contents/Info.plist': {
            'CFBundleDisplayName': 'Product',
            'CFBundleIdentifier': bundle_id,
            'CFBundleName': 'Product',
            'KSProductID': 'test.ksproduct',
            'KSChannelID-full': '-full',
        },
        '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Alerts).app/Contents/Info.plist':
            {
                'CFBundleIdentifier': bundle_id + '.AlertNotificationService'
            },
        '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Alerts).app/Contents/Resources/base.lproj/InfoPlist.strings':
            {
                'CFBundleDisplayName': 'Product'
            },
        '/$W/app-entitlements.plist': {
            'com.apple.application-identifier': bundle_id
        },
        '/$W/helper-renderer-entitlements.plist': {},
        '/$W/helper-gpu-entitlements.plist': {},
        '/$W/helper-plugin-entitlements.plist': {},
        '/$W/App Product Canary.app/Contents/Resources/test.signing.bundle_id.canary.manifest/Contents/Resources/test.signing.bundle_id.canary.manifest':
            {
                'pfm_domain': bundle_id
            }
    }
    plists['/$W/App Product Canary.app/Contents/Info.plist'] = plists[
        '/$W/App Product.app/Contents/Info.plist']
    return plists[args[0]]


def plist_read_with_architecture(*args):
    plist = plist_read(*args)
    plist.update({'KSChannelID': 'arm64', 'KSChannelID-full': 'arm64-full'})
    return plist


@mock.patch('signing.commands.read_plist', side_effect=plist_read)
@mock.patch.multiple(
    'signing.commands', **{
        m: mock.DEFAULT for m in ('copy_files', 'move_file', 'make_dir',
                                  'write_file', 'run_command', 'write_plist')
    })
class TestModification(unittest.TestCase):

    def setUp(self):
        self.paths = model.Paths('/$I', '/$O', '/$W')
        self.config = test_config.TestConfig()

    def _is_framework_unchanged(self, mocks):
        # Determines whether any modifications were made within the framework
        # according to calls to any of the mocked calls in |mocks|. This is done
        # by examining the calls' arguments for a substring pointing into the
        # framework.

        def _do_mock_calls_mention_framework(mock_calls):
            for call in mock_calls:
                for tup in call:
                    for arg in tup:
                        # Don't anchor this substring in a particular directory
                        # because it may appear in any of /$I, /$O, or /$W.
                        # Don't anchor it with App Product.app either, because
                        # it may be renamed (to App Product Canary.app).
                        if 'Contents/Frameworks/Product Framework.framework' in arg:
                            return True

            return False

        for mocked in mocks.values():
            if _do_mock_calls_mention_framework(mocked.mock_calls):
                return False

        return True

    def test_base_distribution(self, read_plist, **kwargs):
        dist = model.Distribution()
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        self.assertEqual(1, kwargs['write_plist'].call_count)
        kwargs['write_plist'].assert_called_with(
            {
                'CFBundleDisplayName': 'Product',
                'CFBundleIdentifier': config.base_bundle_id,
                'CFBundleName': 'Product',
                'KSProductID': 'test.ksproduct',
                'KSChannelID-full': '-full'
            }, '/$W/App Product.app/Contents/Info.plist', 'xml1')

        self.assertEqual(4, kwargs['copy_files'].call_count)
        kwargs['copy_files'].assert_has_calls([
            mock.call('/$I/Product Packaging/app-entitlements.plist',
                      '/$W/app-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-gpu-entitlements.plist',
                      '/$W/helper-gpu-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-plugin-entitlements.plist',
                      '/$W/helper-plugin-entitlements.plist'),
            mock.call(
                '/$I/Product Packaging/helper-renderer-entitlements.plist',
                '/$W/helper-renderer-entitlements.plist'),
        ])
        self.assertEqual(0, kwargs['move_file'].call_count)
        self.assertEqual(0, kwargs['write_file'].call_count)

        self.assertTrue(self._is_framework_unchanged(kwargs))

    def test_distribution_with_architecture(self, read_plist, **kwargs):
        read_plist.side_effect = plist_read_with_architecture

        dist = model.Distribution()
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        self.assertEqual(1, kwargs['write_plist'].call_count)
        kwargs['write_plist'].assert_called_with(
            {
                'CFBundleDisplayName': 'Product',
                'CFBundleIdentifier': config.base_bundle_id,
                'CFBundleName': 'Product',
                'KSProductID': 'test.ksproduct',
                'KSChannelID': 'arm64',
                'KSChannelID-full': 'arm64-full'
            }, '/$W/App Product.app/Contents/Info.plist', 'xml1')

        self.assertEqual(4, kwargs['copy_files'].call_count)
        kwargs['copy_files'].assert_has_calls([
            mock.call('/$I/Product Packaging/app-entitlements.plist',
                      '/$W/app-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-gpu-entitlements.plist',
                      '/$W/helper-gpu-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-plugin-entitlements.plist',
                      '/$W/helper-plugin-entitlements.plist'),
            mock.call(
                '/$I/Product Packaging/helper-renderer-entitlements.plist',
                '/$W/helper-renderer-entitlements.plist'),
        ])
        self.assertEqual(0, kwargs['move_file'].call_count)
        self.assertEqual(0, kwargs['write_file'].call_count)

        self.assertTrue(self._is_framework_unchanged(kwargs))

    def test_distribution_with_brand(self, read_plist, **kwargs):
        dist = model.Distribution(branding_code='MOO')
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        self.assertEqual(1, kwargs['write_plist'].call_count)
        kwargs['write_plist'].assert_called_with(
            {
                'CFBundleDisplayName': 'Product',
                'CFBundleIdentifier': config.base_bundle_id,
                'CFBundleName': 'Product',
                'KSProductID': 'test.ksproduct',
                'KSBrandID': 'MOO',
                'KSChannelID-full': '-full'
            }, '/$W/App Product.app/Contents/Info.plist', 'xml1')

        self.assertEqual(4, kwargs['copy_files'].call_count)
        kwargs['copy_files'].assert_has_calls([
            mock.call('/$I/Product Packaging/app-entitlements.plist',
                      '/$W/app-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-gpu-entitlements.plist',
                      '/$W/helper-gpu-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-plugin-entitlements.plist',
                      '/$W/helper-plugin-entitlements.plist'),
            mock.call(
                '/$I/Product Packaging/helper-renderer-entitlements.plist',
                '/$W/helper-renderer-entitlements.plist'),
        ])
        self.assertEqual(0, kwargs['move_file'].call_count)

        self.assertTrue(self._is_framework_unchanged(kwargs))

    def test_distribution_with_channel(self, read_plist, **kwargs):
        dist = model.Distribution(channel='dev')
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        self.assertEqual(1, kwargs['write_plist'].call_count)
        kwargs['write_plist'].assert_called_with(
            {
                'CFBundleDisplayName': 'Product',
                'CFBundleIdentifier': config.base_bundle_id,
                'CFBundleName': 'Product',
                'KSProductID': 'test.ksproduct',
                'KSChannelID': 'dev',
                'KSChannelID-full': 'dev-full'
            }, '/$W/App Product.app/Contents/Info.plist', 'xml1')

        self.assertEqual(4, kwargs['copy_files'].call_count)
        kwargs['copy_files'].assert_has_calls([
            mock.call('/$I/Product Packaging/app-entitlements.plist',
                      '/$W/app-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-gpu-entitlements.plist',
                      '/$W/helper-gpu-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-plugin-entitlements.plist',
                      '/$W/helper-plugin-entitlements.plist'),
            mock.call(
                '/$I/Product Packaging/helper-renderer-entitlements.plist',
                '/$W/helper-renderer-entitlements.plist'),
        ])
        self.assertEqual(0, kwargs['move_file'].call_count)
        self.assertEqual(0, kwargs['write_file'].call_count)

        self.assertTrue(self._is_framework_unchanged(kwargs))

    def test_distribution_with_architecture_and_channel(self, read_plist,
                                                        **kwargs):
        read_plist.side_effect = plist_read_with_architecture

        dist = model.Distribution(channel='dev')
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        self.assertEqual(1, kwargs['write_plist'].call_count)
        kwargs['write_plist'].assert_called_with(
            {
                'CFBundleDisplayName': 'Product',
                'CFBundleIdentifier': config.base_bundle_id,
                'CFBundleName': 'Product',
                'KSProductID': 'test.ksproduct',
                'KSChannelID': 'arm64-dev',
                'KSChannelID-full': 'arm64-dev-full'
            }, '/$W/App Product.app/Contents/Info.plist', 'xml1')

        self.assertEqual(4, kwargs['copy_files'].call_count)
        kwargs['copy_files'].assert_has_calls([
            mock.call('/$I/Product Packaging/app-entitlements.plist',
                      '/$W/app-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-gpu-entitlements.plist',
                      '/$W/helper-gpu-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-plugin-entitlements.plist',
                      '/$W/helper-plugin-entitlements.plist'),
            mock.call(
                '/$I/Product Packaging/helper-renderer-entitlements.plist',
                '/$W/helper-renderer-entitlements.plist'),
        ])
        self.assertEqual(0, kwargs['move_file'].call_count)
        self.assertEqual(0, kwargs['write_file'].call_count)

        self.assertTrue(self._is_framework_unchanged(kwargs))

    def test_distribution_with_product_dirname(self, read_plist, **kwargs):
        dist = model.Distribution(product_dirname='Farmland/Cows')
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        self.assertEqual(1, kwargs['write_plist'].call_count)
        kwargs['write_plist'].assert_called_with(
            {
                'CFBundleDisplayName': 'Product',
                'CFBundleIdentifier': config.base_bundle_id,
                'CFBundleName': 'Product',
                'KSProductID': 'test.ksproduct',
                'KSChannelID-full': '-full',
                'CrProductDirName': 'Farmland/Cows'
            }, '/$W/App Product.app/Contents/Info.plist', 'xml1')

        self.assertEqual(4, kwargs['copy_files'].call_count)
        kwargs['copy_files'].assert_has_calls([
            mock.call('/$I/Product Packaging/app-entitlements.plist',
                      '/$W/app-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-gpu-entitlements.plist',
                      '/$W/helper-gpu-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-plugin-entitlements.plist',
                      '/$W/helper-plugin-entitlements.plist'),
            mock.call(
                '/$I/Product Packaging/helper-renderer-entitlements.plist',
                '/$W/helper-renderer-entitlements.plist'),
        ])
        self.assertEqual(0, kwargs['move_file'].call_count)
        self.assertEqual(0, kwargs['write_file'].call_count)

        self.assertTrue(self._is_framework_unchanged(kwargs))

    def test_distribution_with_creator_code(self, read_plist, **kwargs):
        dist = model.Distribution(creator_code='Mooo')
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        self.assertEqual(1, kwargs['write_plist'].call_count)
        kwargs['write_plist'].assert_called_with(
            {
                'CFBundleDisplayName': 'Product',
                'CFBundleIdentifier': config.base_bundle_id,
                'CFBundleName': 'Product',
                'KSProductID': 'test.ksproduct',
                'KSChannelID-full': '-full',
                'CFBundleSignature': 'Mooo'
            }, '/$W/App Product.app/Contents/Info.plist', 'xml1')

        self.assertEqual(4, kwargs['copy_files'].call_count)
        kwargs['copy_files'].assert_has_calls([
            mock.call('/$I/Product Packaging/app-entitlements.plist',
                      '/$W/app-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-gpu-entitlements.plist',
                      '/$W/helper-gpu-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-plugin-entitlements.plist',
                      '/$W/helper-plugin-entitlements.plist'),
            mock.call(
                '/$I/Product Packaging/helper-renderer-entitlements.plist',
                '/$W/helper-renderer-entitlements.plist'),
        ])
        kwargs['write_file'].assert_called_once_with(
            '/$W/App Product.app/Contents/PkgInfo', 'APPLMooo')
        self.assertEqual(0, kwargs['move_file'].call_count)

    def test_distribution_with_brand_and_channel(self, read_plist, **kwargs):
        dist = model.Distribution(channel='beta', branding_code='RAWR')
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        self.assertEqual(1, kwargs['write_plist'].call_count)
        kwargs['write_plist'].assert_called_with(
            {
                'CFBundleDisplayName': 'Product',
                'CFBundleIdentifier': config.base_bundle_id,
                'CFBundleName': 'Product',
                'KSProductID': 'test.ksproduct',
                'KSChannelID': 'beta',
                'KSChannelID-full': 'beta-full',
                'KSBrandID': 'RAWR'
            }, '/$W/App Product.app/Contents/Info.plist', 'xml1')

        self.assertEqual(4, kwargs['copy_files'].call_count)
        kwargs['copy_files'].assert_has_calls([
            mock.call('/$I/Product Packaging/app-entitlements.plist',
                      '/$W/app-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-gpu-entitlements.plist',
                      '/$W/helper-gpu-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-plugin-entitlements.plist',
                      '/$W/helper-plugin-entitlements.plist'),
            mock.call(
                '/$I/Product Packaging/helper-renderer-entitlements.plist',
                '/$W/helper-renderer-entitlements.plist'),
        ])
        self.assertEqual(0, kwargs['move_file'].call_count)
        self.assertEqual(0, kwargs['write_file'].call_count)

    def test_customize_channel(self, read_plist, **kwargs):
        dist = model.Distribution(
            channel='canary',
            app_name_fragment='Canary',
            product_dirname='Acme/Product Canary',
            creator_code='Mooo',
            channel_customize=True)
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        # Order of file moves is significant.
        self.assertEqual(kwargs['move_file'].mock_calls, [
            mock.call('/$W/App Product.app', '/$W/App Product Canary.app'),
            mock.call(
                '/$W/App Product Canary.app/Contents/MacOS/App Product',
                '/$W/App Product Canary.app/Contents/MacOS/App Product Canary'),
            mock.call(
                '/$W/App Product Canary.app/Contents/Resources/test.signing.bundle_id.manifest/Contents/Resources/test.signing.bundle_id.manifest',
                '/$W/App Product Canary.app/Contents/Resources/test.signing.bundle_id.manifest/Contents/Resources/test.signing.bundle_id.canary.manifest'
            ),
            mock.call(
                '/$W/App Product Canary.app/Contents/Resources/test.signing.bundle_id.manifest',
                '/$W/App Product Canary.app/Contents/Resources/test.signing.bundle_id.canary.manifest'
            ),
        ])

        self.assertEqual(7, kwargs['copy_files'].call_count)
        kwargs['copy_files'].assert_has_calls([
            mock.call('/$I/Product Packaging/app-entitlements.plist',
                      '/$W/app-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-gpu-entitlements.plist',
                      '/$W/helper-gpu-entitlements.plist'),
            mock.call('/$I/Product Packaging/helper-plugin-entitlements.plist',
                      '/$W/helper-plugin-entitlements.plist'),
            mock.call(
                '/$I/Product Packaging/helper-renderer-entitlements.plist',
                '/$W/helper-renderer-entitlements.plist'),
            mock.call('/$I/Product Packaging/app_canary.icns',
                      '/$W/App Product Canary.app/Contents/Resources/app.icns'),
            mock.call(
                '/$I/Product Packaging/document_canary.icns',
                '/$W/App Product Canary.app/Contents/Resources/document.icns'),
            mock.call(
                '/$I/Product Packaging/app_canary.icns',
                '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Alerts).app/Contents/Resources/app.icns'
            )
        ])
        kwargs['write_file'].assert_called_once_with(
            '/$W/App Product Canary.app/Contents/PkgInfo', 'APPLMooo')

        self.assertEqual(8, kwargs['write_plist'].call_count)
        kwargs['write_plist'].assert_has_calls([
            mock.call(
                {
                    'CFBundleIdentifier':
                        'test.signing.bundle_id.canary.AlertNotificationService'
                },
                '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Alerts).app/Contents/Info.plist',
                'xml1'),
            mock.call({
                'CFBundleDisplayName': 'Product Canary'
            }, '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Alerts).app/Contents/Resources/base.lproj/InfoPlist.strings',
                      'binary1'),
            mock.call(
                {
                    'CFBundleDisplayName': 'Product Canary',
                    'CFBundleIdentifier': config.base_bundle_id,
                    'CFBundleExecutable': config.app_product,
                    'CFBundleName': 'Product Canary',
                    'KSProductID': 'test.ksproduct.canary',
                    'KSChannelID': 'canary',
                    'KSChannelID-full': 'canary-full',
                    'CrProductDirName': 'Acme/Product Canary',
                    'CFBundleSignature': 'Mooo'
                }, '/$W/App Product Canary.app/Contents/Info.plist', 'xml1'),
            mock.call(
                {
                    'com.apple.application-identifier':
                        'test.signing.bundle_id.canary'
                }, '/$W/app-entitlements.plist', 'xml1'),
            mock.call({}, '/$W/helper-gpu-entitlements.plist', 'xml1'),
            mock.call({}, '/$W/helper-plugin-entitlements.plist', 'xml1'),
            mock.call({}, '/$W/helper-renderer-entitlements.plist', 'xml1'),
            mock.call({
                'pfm_domain': 'test.signing.bundle_id.canary'
            }, '/$W/App Product Canary.app/Contents/Resources/test.signing.bundle_id.canary.manifest/Contents/Resources/test.signing.bundle_id.canary.manifest',
                      'xml1')
        ])

        self.assertFalse(self._is_framework_unchanged(kwargs))

    def test_get_task_allow_no_channel_customize(self, read_plist, **kwargs):
        dist = model.Distribution()
        self.config = test_config.TestConfigInjectGetTaskAllow()
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        self.assertEqual(5, kwargs['write_plist'].call_count)
        kwargs['write_plist'].assert_has_calls([
            mock.call(
                {
                    'CFBundleDisplayName': 'Product',
                    'CFBundleIdentifier': config.base_bundle_id,
                    'CFBundleName': 'Product',
                    'KSProductID': 'test.ksproduct',
                    'KSChannelID-full': '-full'
                }, '/$W/App Product.app/Contents/Info.plist', 'xml1'),
            mock.call(
                {
                    'com.apple.security.get-task-allow': True,
                    'com.apple.application-identifier': config.base_bundle_id
                }, '/$W/app-entitlements.plist', 'xml1'),
            mock.call({'com.apple.security.get-task-allow': True},
                      '/$W/helper-gpu-entitlements.plist', 'xml1'),
            mock.call({'com.apple.security.get-task-allow': True},
                      '/$W/helper-plugin-entitlements.plist', 'xml1'),
            mock.call({'com.apple.security.get-task-allow': True},
                      '/$W/helper-renderer-entitlements.plist', 'xml1'),
        ])

    def test_get_task_allow_customize_channel(self, read_plist, **kwargs):
        dist = model.Distribution(
            channel='canary',
            app_name_fragment='Canary',
            product_dirname='Acme/Product Canary',
            creator_code='Mooo',
            channel_customize=True)
        self.config = test_config.TestConfigInjectGetTaskAllow()
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        self.assertEqual(8, kwargs['write_plist'].call_count)
        kwargs['write_plist'].assert_has_calls([
            mock.call(
                {
                    'CFBundleIdentifier':
                        'test.signing.bundle_id.canary.AlertNotificationService'
                },
                '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Alerts).app/Contents/Info.plist',
                'xml1'),
            mock.call({
                'CFBundleDisplayName': 'Product Canary'
            }, '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Alerts).app/Contents/Resources/base.lproj/InfoPlist.strings',
                      'binary1'),
            mock.call(
                {
                    'CFBundleDisplayName': 'Product Canary',
                    'CFBundleIdentifier': config.base_bundle_id,
                    'CFBundleExecutable': config.app_product,
                    'CFBundleName': 'Product Canary',
                    'KSProductID': 'test.ksproduct.canary',
                    'KSChannelID': 'canary',
                    'KSChannelID-full': 'canary-full',
                    'CrProductDirName': 'Acme/Product Canary',
                    'CFBundleSignature': 'Mooo'
                }, '/$W/App Product Canary.app/Contents/Info.plist', 'xml1'),
            mock.call(
                {
                    'com.apple.security.get-task-allow':
                        True,
                    'com.apple.application-identifier':
                        'test.signing.bundle_id.canary'
                }, '/$W/app-entitlements.plist', 'xml1'),
            mock.call({'com.apple.security.get-task-allow': True},
                      '/$W/helper-gpu-entitlements.plist', 'xml1'),
            mock.call({'com.apple.security.get-task-allow': True},
                      '/$W/helper-plugin-entitlements.plist', 'xml1'),
            mock.call({'com.apple.security.get-task-allow': True},
                      '/$W/helper-renderer-entitlements.plist', 'xml1'),
            mock.call({
                'pfm_domain': 'test.signing.bundle_id.canary'
            }, '/$W/App Product Canary.app/Contents/Resources/test.signing.bundle_id.canary.manifest/Contents/Resources/test.signing.bundle_id.canary.manifest',
                      'xml1')
        ])
