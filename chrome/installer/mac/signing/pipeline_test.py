# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import unittest
from unittest import mock
from xml.etree import ElementTree

from signing import model, pipeline, test_config


def _get_work_dir(*args, **kwargs):
    _get_work_dir.count += 1
    return '/$W_{}'.format(_get_work_dir.count)


_get_work_dir.count = 0


def _component_property_path(paths, dist_config):
    return '/$W_1/App Product.plist'


def _productbuild_distribution_path(ap, pp, d, c):
    return '/$W_1/App Product.dist'


def _create_pkgbuild_scripts(p, d):
    return '/$W_1/scripts'


def _macos_version_pre_12():
    return [11, 0]


def _macos_version_12():
    return [12, 0]


def _minimum_os_version(a, d):
    return '10.11.0'


def _read_plist(p):
    return {'LSMinimumSystemVersion': '10.19.7'}


def _write_plist(d, p, f):
    _write_plist.contents = d


_write_plist.contents = ''


def _last_written_plist():
    return _write_plist.contents


def _run_command_output_lipo(b):
    return b'x86_64,arm64'


def _read_file(p):
    if p == '/$I/Product Packaging/pkg_postinstall.in':
        return """app dir is '@APP_DIR@'
app product is '@APP_PRODUCT@'
brand code is '@BRAND_CODE@'
framework dir is '@FRAMEWORK_DIR@'"""

    raise


def _get_adjacent_item(l, o):
    """Finds object |o| in collection |l| and returns the item at its index
    plus 1.
    """
    index = l.index(o)
    return l[index + 1]


def _filter_distributions(d, b, c):
    _filter_distributions.brands = b
    _filter_distributions.channels = c
    return d


_filter_distributions.brands = None
_filter_distributions.channels = None


def _last_brand_filter():
    return _filter_distributions.brands


def _last_channel_filter():
    return _filter_distributions.channels


@mock.patch.multiple(
    'signing.commands', **{
        m: mock.DEFAULT
        for m in ('move_file', 'copy_files',
                  'copy_dir_overwrite_and_count_changes', 'run_command',
                  'make_dir', 'shutil', 'write_file', 'set_executable')
    })
@mock.patch.multiple('signing.signing',
                     **{m: mock.DEFAULT for m in ('sign_part', 'verify_part')})
@mock.patch.multiple('signing.parts', **{'sign_chrome': mock.DEFAULT})
@mock.patch('signing.commands.tempfile.mkdtemp', _get_work_dir)
class TestPipelineHelpers(unittest.TestCase):

    def setUp(self):
        _get_work_dir.count = 0
        self.paths = model.Paths('/$I', '/$O', None)

    @mock.patch('signing.modification.customize_distribution')
    def test_customize_and_sign_chrome_no_customize(self, customize, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)
        manager.attach_mock(customize, 'customize_distribution')

        dist = model.Distribution()
        config = test_config.TestConfig()
        dist_config = dist.to_config(config)
        paths = self.paths.replace_work('/$W')

        pipeline._customize_and_sign_chrome(paths, dist_config, '$D', None)

        manager.assert_has_calls([
            mock.call.copy_files('/$I/App Product.app', '/$W'),
            mock.call.customize_distribution(paths, dist, dist_config),
            mock.call.copy_dir_overwrite_and_count_changes(
                '/$W/App Product.app/Contents/Frameworks/Product Framework.framework',
                '/$W/modified_unsigned_framework',
                dry_run=False),
            mock.call.sign_chrome(paths, dist_config, sign_framework=True),
            mock.call.copy_dir_overwrite_and_count_changes(
                '/$W/App Product.app/Contents/Frameworks/Product Framework.framework',
                '/$W/modified_unsigned_framework',
                dry_run=True),
            mock.call.make_dir('$D'),
            mock.call.move_file('/$W/App Product.app', '$D/App Product.app')
        ])

    @mock.patch('signing.modification.customize_distribution')
    def test_customize_and_sign_chrome_customize_different_product(
            self, customize, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)
        manager.attach_mock(customize, 'customize_distribution')

        dist = model.Distribution(
            channel_customize=True,
            channel='canary',
            app_name_fragment='Canary',
            product_dirname='canary',
            creator_code='cana')
        config = test_config.TestConfig()
        dist_config = dist.to_config(config)
        paths = self.paths.replace_work('/$W')

        pipeline._customize_and_sign_chrome(paths, dist_config, '$D', None)

        manager.assert_has_calls([
            mock.call.copy_files('/$I/App Product.app', '/$W'),
            mock.call.customize_distribution(paths, dist, dist_config),
            mock.call.copy_dir_overwrite_and_count_changes(
                '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework',
                '/$W/modified_unsigned_framework',
                dry_run=False),
            mock.call.sign_chrome(paths, dist_config, sign_framework=True),
            mock.call.copy_dir_overwrite_and_count_changes(
                '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework',
                '/$W/modified_unsigned_framework',
                dry_run=True),
            mock.call.make_dir('$D'),
            mock.call.move_file('/$W/App Product Canary.app',
                                '$D/App Product Canary.app')
        ])

    @mock.patch('signing.modification.customize_distribution')
    def test_customize_and_sign_chrome_customize_multiple_times(
            self, customize, **kwargs):
        kwargs['copy_dir_overwrite_and_count_changes'].return_value = 0

        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)
        manager.attach_mock(customize, 'customize_distribution')

        config = test_config.TestConfig()

        base_dist = model.Distribution()
        base_dist_config = base_dist.to_config(config)
        paths = self.paths.replace_work('/$W')

        notary_paths = paths.replace_work('/$D')

        signed_frameworks = {}
        pipeline._customize_and_sign_chrome(paths, base_dist_config,
                                            notary_paths.work,
                                            signed_frameworks)

        branded_dist = model.Distribution(
            branding_code='c0de', packaging_name_fragment='Branded')
        branded_dist_config = branded_dist.to_config(config)
        paths = self.paths.replace_work('/$W')

        pipeline._customize_and_sign_chrome(paths, branded_dist_config,
                                            notary_paths.work,
                                            signed_frameworks)

        channel_dist = model.Distribution(
            channel_customize=True,
            channel='canary',
            app_name_fragment='Canary',
            product_dirname='canary',
            creator_code='cana')
        channel_dist_config = channel_dist.to_config(config)
        paths = self.paths.replace_work('/$W')

        pipeline._customize_and_sign_chrome(paths, channel_dist_config,
                                            notary_paths.work,
                                            signed_frameworks)

        manager.assert_has_calls([
            mock.call.copy_files('/$I/App Product.app', '/$W'),
            mock.call.customize_distribution(paths, base_dist,
                                             base_dist_config),
            mock.call.copy_dir_overwrite_and_count_changes(
                '/$W/App Product.app/Contents/Frameworks/Product Framework.framework',
                '/$W/modified_unsigned_framework',
                dry_run=False),
            mock.call.sign_chrome(paths, base_dist_config, sign_framework=True),
            mock.call.copy_dir_overwrite_and_count_changes(
                '/$W/App Product.app/Contents/Frameworks/Product Framework.framework',
                '/$W/modified_unsigned_framework',
                dry_run=True),
            mock.call.make_dir('/$D'),
            mock.call.move_file('/$W/App Product.app', '/$D/App Product.app'),
            mock.call.copy_files('/$I/App Product.app', '/$W'),
            mock.call.customize_distribution(paths, branded_dist,
                                             branded_dist_config),
            mock.call.copy_dir_overwrite_and_count_changes(
                '/$D/App Product.app/Contents/Frameworks/Product Framework.framework',
                '/$W/App Product.app/Contents/Frameworks/Product Framework.framework',
                dry_run=False),
            mock.call.sign_chrome(
                paths, branded_dist_config, sign_framework=False),
            mock.call.make_dir('/$D'),
            mock.call.move_file('/$W/App Product.app', '/$D/App Product.app'),
            mock.call.copy_files('/$I/App Product.app', '/$W'),
            mock.call.customize_distribution(paths, channel_dist,
                                             channel_dist_config),
            mock.call.copy_dir_overwrite_and_count_changes(
                '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework',
                '/$W/modified_unsigned_framework',
                dry_run=False),
            mock.call.sign_chrome(
                paths, channel_dist_config, sign_framework=True),
            mock.call.copy_dir_overwrite_and_count_changes(
                '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework',
                '/$W/modified_unsigned_framework',
                dry_run=True),
            mock.call.make_dir('/$D'),
            mock.call.move_file('/$W/App Product Canary.app',
                                '/$D/App Product Canary.app')
        ])

    @mock.patch('signing.notarize.staple')
    def test_staple_chrome_no_customize(self, staple, **kwargs):
        dist = model.Distribution()
        paths = self.paths.replace_work('/$W')

        pipeline._staple_chrome(paths, dist.to_config(test_config.TestConfig()))

        self.assertEqual(staple.mock_calls, [
            mock.call(
                '/$W/App Product.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper.app'
            ),
            mock.call(
                '/$W/App Product.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Renderer).app'
            ),
            mock.call(
                '/$W/App Product.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Plugin).app'
            ),
            mock.call(
                '/$W/App Product.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (GPU).app'
            ),
            mock.call(
                '/$W/App Product.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Alerts).app'
            ),
            mock.call('/$W/App Product.app')
        ])

    @mock.patch('signing.notarize.staple')
    def test_staple_chrome_customize(self, staple, **kwargs):
        dist = model.Distribution(
            channel_customize=True,
            channel='canary',
            app_name_fragment='Canary',
            product_dirname='canary',
            creator_code='cana')
        paths = self.paths.replace_work('/$W')

        pipeline._staple_chrome(paths, dist.to_config(test_config.TestConfig()))

        self.assertEqual(staple.mock_calls, [
            mock.call(
                '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper.app'
            ),
            mock.call(
                '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Renderer).app'
            ),
            mock.call(
                '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Plugin).app'
            ),
            mock.call(
                '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (GPU).app'
            ),
            mock.call(
                '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Alerts).app'
            ),
            mock.call('/$W/App Product Canary.app')
        ])

    @mock.patch('signing.commands.read_file', _read_file)
    def test_create_pkgbuild_scripts(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        dist = model.Distribution(
            branding_code='MOO',
            packaging_name_fragment='ForCows',
            package_as_dmg=False,
            package_as_pkg=True,
            package_as_zip=False)
        dist_config = dist.to_config(test_config.TestConfig())
        paths = self.paths.replace_work('/$W')

        self.assertEqual('/$W/scripts',
                         pipeline._create_pkgbuild_scripts(paths, dist_config))

        manager.assert_has_calls([
            mock.call.make_dir('/$W/scripts'),
            mock.call.write_file('/$W/scripts/postinstall', mock.ANY),
            mock.call.set_executable('/$W/scripts/postinstall')
        ])

        postinstall_string = manager.mock_calls[1][1][1]
        self.assertEqual(
            postinstall_string, """app dir is 'App Product.app'
app product is 'App Product'
brand code is 'MOO'
framework dir is 'App Product.app/Contents/Frameworks/Product Framework.framework'"""
        )

    @mock.patch('signing.pipeline.commands.write_plist', _write_plist)
    def test_component_property_path(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        dist = model.Distribution()
        dist_config = dist.to_config(test_config.TestConfig())
        paths = self.paths.replace_work('/$W')

        self.assertEqual('/$W/App Product.plist',
                         pipeline._component_property_path(paths, dist_config))

        self.assertEqual(_last_written_plist(), [{
            'BundleOverwriteAction': 'upgrade',
            'BundleIsVersionChecked': True,
            'BundleHasStrictIdentifier': True,
            'RootRelativeBundlePath': 'App Product.app',
            'BundleIsRelocatable': False
        }])

    @mock.patch('signing.commands.read_plist', _read_plist)
    @mock.patch('signing.commands.run_command_output', _run_command_output_lipo)
    def test_productbuild_distribution_path(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        dist = model.Distribution()
        dist_config = dist.to_config(test_config.TestConfig())
        paths = self.paths.replace_work('/$W')

        component_pkg_path = os.path.join(
            paths.work, '{}.pkg'.format(dist_config.app_product))

        self.assertEqual(
            '/$W/App Product.dist',
            pipeline._productbuild_distribution_path(paths, paths, dist_config,
                                                     component_pkg_path))

        manager.assert_has_calls([
            mock.call.write_file('/$W/App Product.dist', mock.ANY),
        ])

        xml_string = manager.mock_calls[0][1][1]

        xml_root = ElementTree.fromstring(xml_string)
        volume_check = xml_root.find('volume-check')
        allowed_os_versions = volume_check.find('allowed-os-versions')
        os_versions = allowed_os_versions.findall('os-version')
        self.assertEqual(len(os_versions), 1)
        os_version = os_versions[0].get('min')
        self.assertEqual(os_version, '10.19.7')

    def test_package_and_sign_dmg_no_branding(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig()
        dist = model.Distribution(
            package_as_dmg=True, package_as_pkg=False, package_as_zip=False)
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual('/$O/AppProduct-99.0.9999.99.dmg',
                         pipeline._package_and_sign_dmg(paths, dist_config))

        manager.assert_has_calls([
            mock.call.make_dir('/$W/empty'),
            mock.call.run_command(mock.ANY),
            mock.call.sign_part(paths, dist_config, mock.ANY),
            mock.call.verify_part(paths, mock.ANY)
        ])

        run_command = [
            call for call in manager.mock_calls if call[0] == 'run_command'
        ][0]
        pkg_dmg_args = run_command[1][0]

        self.assertEqual('/$O/AppProduct-99.0.9999.99.dmg',
                         _get_adjacent_item(pkg_dmg_args, '--target'))
        self.assertEqual(config.app_product,
                         _get_adjacent_item(pkg_dmg_args, '--volname'))
        self.assertEqual('AppProduct-99.0.9999.99',
                         kwargs['sign_part'].mock_calls[0][1][2].identifier)

    def test_package_zip_chrome_branded(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig()
        dist = model.Distribution(
            package_as_dmg=False, package_as_pkg=False, package_as_zip=True)
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual('/$O/AppProduct-99.0.9999.99.zip',
                         pipeline._package_zip(paths, dist_config))

        manager.assert_has_calls([
            mock.call.copy_files('/$I/Product Packaging/keystone_install.sh',
                                 '/$W/.keystone_install'),
            mock.call.run_command([
                'zip', '-9', '--recurse-paths', '--symlinks', '--quiet',
                '/$O/AppProduct-99.0.9999.99.zip', 'App Product.app',
                '.keystone_install'
            ],
                                  cwd='/$W'),
        ])

    def test_package_zip_non_chrome_branded(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfigNonChromeBranded()
        dist = model.Distribution(
            package_as_dmg=False, package_as_pkg=False, package_as_zip=True)
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual('/$O/AppProduct-99.0.9999.99.zip',
                         pipeline._package_zip(paths, dist_config))

        manager.assert_has_calls([
            mock.call.run_command([
                'zip', '-9', '--recurse-paths', '--symlinks', '--quiet',
                '/$O/AppProduct-99.0.9999.99.zip', 'App Product.app'
            ],
                                  cwd='/$W'),
        ])

    def test_package_zip_chrome_branded_with_fragment(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig()
        dist = model.Distribution(
            packaging_name_fragment='imagination',
            package_as_dmg=False,
            package_as_pkg=False,
            package_as_zip=True)
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual('/$O/AppProduct-99.0.9999.99-imagination.zip',
                         pipeline._package_zip(paths, dist_config))

        manager.assert_has_calls([
            mock.call.copy_files('/$I/Product Packaging/keystone_install.sh',
                                 '/$W/.keystone_install'),
            mock.call.run_command([
                'zip', '-9', '--recurse-paths', '--symlinks', '--quiet',
                '/$O/AppProduct-99.0.9999.99-imagination.zip',
                'App Product.app', '.keystone_install'
            ],
                                  cwd='/$W'),
        ])

    def test_package_zip_non_chrome_branded_with_fragment(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfigNonChromeBranded()
        dist = model.Distribution(
            packaging_name_fragment='imagination',
            package_as_dmg=False,
            package_as_pkg=False,
            package_as_zip=True)
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual('/$O/AppProduct-99.0.9999.99-imagination.zip',
                         pipeline._package_zip(paths, dist_config))

        manager.assert_has_calls([
            mock.call.run_command([
                'zip', '-9', '--recurse-paths', '--symlinks', '--quiet',
                '/$O/AppProduct-99.0.9999.99-imagination.zip', 'App Product.app'
            ],
                                  cwd='/$W')
        ])

    @mock.patch('signing.pipeline._component_property_path',
                _component_property_path)
    @mock.patch('signing.pipeline._productbuild_distribution_path',
                _productbuild_distribution_path)
    @mock.patch('signing.pipeline._create_pkgbuild_scripts',
                _create_pkgbuild_scripts)
    @mock.patch('signing.commands.macos_version', _macos_version_pre_12)
    def test_package_and_sign_pkg_no_branding_pre_12(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig()
        dist = model.Distribution(
            package_as_dmg=False, package_as_pkg=True, package_as_zip=False)
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual('/$O/AppProduct-99.0.9999.99.pkg',
                         pipeline._package_and_sign_pkg(paths, dist_config))

        manager.assert_has_calls(
            [mock.call.run_command(mock.ANY),
             mock.call.run_command(mock.ANY)])

        run_commands = [
            call for call in manager.mock_calls if call[0] == 'run_command'
        ]
        pkgbuild_args = run_commands[0][1][0]
        productbuild_args = run_commands[1][1][0]

        self.assertEqual('/$W_1/payload',
                         _get_adjacent_item(pkgbuild_args, '--root'))
        self.assertEqual('/$W_1/App Product.plist',
                         _get_adjacent_item(pkgbuild_args, '--component-plist'))
        self.assertEqual('test.signing.bundle_id',
                         _get_adjacent_item(pkgbuild_args, '--identifier'))
        self.assertEqual('99.0.9999.99',
                         _get_adjacent_item(pkgbuild_args, '--version'))
        self.assertEqual('/$W_1/scripts',
                         _get_adjacent_item(pkgbuild_args, '--scripts'))

        self.assertNotIn('--compression', pkgbuild_args)
        self.assertNotIn('--min-os-version', pkgbuild_args)

        self.assertEqual('test.signing.bundle_id',
                         _get_adjacent_item(productbuild_args, '--identifier'))
        self.assertEqual('99.0.9999.99',
                         _get_adjacent_item(productbuild_args, '--version'))
        self.assertEqual(
            '/$W_1/App Product.dist',
            _get_adjacent_item(productbuild_args, '--distribution'))
        self.assertEqual(
            '/$W_1', _get_adjacent_item(productbuild_args, '--package-path'))
        self.assertEqual('[INSTALLER-IDENTITY]',
                         _get_adjacent_item(productbuild_args, '--sign'))

    @mock.patch('signing.pipeline._component_property_path',
                _component_property_path)
    @mock.patch('signing.pipeline._productbuild_distribution_path',
                _productbuild_distribution_path)
    @mock.patch('signing.pipeline._create_pkgbuild_scripts',
                _create_pkgbuild_scripts)
    @mock.patch('signing.commands.macos_version', _macos_version_12)
    @mock.patch('signing.pipeline._minimum_os_version', _minimum_os_version)
    def test_package_and_sign_pkg_no_branding_12(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig()
        dist = model.Distribution(
            package_as_dmg=False, package_as_pkg=True, package_as_zip=False)
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual('/$O/AppProduct-99.0.9999.99.pkg',
                         pipeline._package_and_sign_pkg(paths, dist_config))

        manager.assert_has_calls(
            [mock.call.run_command(mock.ANY),
             mock.call.run_command(mock.ANY)])

        run_commands = [
            call for call in manager.mock_calls if call[0] == 'run_command'
        ]
        pkgbuild_args = run_commands[0][1][0]
        productbuild_args = run_commands[1][1][0]

        self.assertEqual('/$W_1/payload',
                         _get_adjacent_item(pkgbuild_args, '--root'))
        self.assertEqual('/$W_1/App Product.plist',
                         _get_adjacent_item(pkgbuild_args, '--component-plist'))
        self.assertEqual('test.signing.bundle_id',
                         _get_adjacent_item(pkgbuild_args, '--identifier'))
        self.assertEqual('99.0.9999.99',
                         _get_adjacent_item(pkgbuild_args, '--version'))
        self.assertEqual('/$W_1/scripts',
                         _get_adjacent_item(pkgbuild_args, '--scripts'))

        self.assertEqual('latest',
                         _get_adjacent_item(pkgbuild_args, '--compression'))
        self.assertEqual('10.11.0',
                         _get_adjacent_item(pkgbuild_args, '--min-os-version'))

        self.assertEqual('test.signing.bundle_id',
                         _get_adjacent_item(productbuild_args, '--identifier'))
        self.assertEqual('99.0.9999.99',
                         _get_adjacent_item(productbuild_args, '--version'))
        self.assertEqual(
            '/$W_1/App Product.dist',
            _get_adjacent_item(productbuild_args, '--distribution'))
        self.assertEqual(
            '/$W_1', _get_adjacent_item(productbuild_args, '--package-path'))
        self.assertEqual('[INSTALLER-IDENTITY]',
                         _get_adjacent_item(productbuild_args, '--sign'))

    def test_package_and_sign_dmg_branding(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig()
        dist = model.Distribution(
            branding_code='MOO',
            packaging_name_fragment='ForCows',
            package_as_dmg=True,
            package_as_pkg=False,
            package_as_zip=False)
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual('/$O/AppProduct-99.0.9999.99-ForCows.dmg',
                         pipeline._package_and_sign_dmg(paths, dist_config))

        manager.assert_has_calls([
            mock.call.make_dir('/$W/empty'),
            mock.call.run_command(mock.ANY),
            mock.call.sign_part(paths, dist_config, mock.ANY),
            mock.call.verify_part(paths, mock.ANY)
        ])

        run_command = [
            call for call in manager.mock_calls if call[0] == 'run_command'
        ][0]
        pkg_dmg_args = run_command[1][0]

        self.assertEqual('/$O/AppProduct-99.0.9999.99-ForCows.dmg',
                         _get_adjacent_item(pkg_dmg_args, '--target'))
        self.assertEqual(config.app_product,
                         _get_adjacent_item(pkg_dmg_args, '--volname'))
        self.assertEqual('AppProduct-99.0.9999.99-MOO',
                         kwargs['sign_part'].mock_calls[0][1][2].identifier)

    @mock.patch('signing.pipeline._component_property_path',
                _component_property_path)
    @mock.patch('signing.pipeline._productbuild_distribution_path',
                _productbuild_distribution_path)
    @mock.patch('signing.pipeline._create_pkgbuild_scripts',
                _create_pkgbuild_scripts)
    @mock.patch('signing.commands.macos_version', _macos_version_pre_12)
    def test_package_and_sign_pkg_branding_pre12(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig()
        dist = model.Distribution(
            branding_code='MOO',
            packaging_name_fragment='ForCows',
            package_as_dmg=False,
            package_as_pkg=True,
            package_as_zip=False)
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual('/$O/AppProduct-99.0.9999.99-ForCows.pkg',
                         pipeline._package_and_sign_pkg(paths, dist_config))

        manager.assert_has_calls(
            [mock.call.run_command(mock.ANY),
             mock.call.run_command(mock.ANY)])

        run_commands = [
            call for call in manager.mock_calls if call[0] == 'run_command'
        ]
        pkgbuild_args = run_commands[0][1][0]
        productbuild_args = run_commands[1][1][0]

        self.assertEqual('/$W_1/payload',
                         _get_adjacent_item(pkgbuild_args, '--root'))
        self.assertEqual('/$W_1/App Product.plist',
                         _get_adjacent_item(pkgbuild_args, '--component-plist'))
        self.assertEqual('test.signing.bundle_id',
                         _get_adjacent_item(pkgbuild_args, '--identifier'))
        self.assertEqual('99.0.9999.99',
                         _get_adjacent_item(pkgbuild_args, '--version'))
        self.assertEqual('/$W_1/scripts',
                         _get_adjacent_item(pkgbuild_args, '--scripts'))

        self.assertNotIn('--compression', pkgbuild_args)
        self.assertNotIn('--min-os-version', pkgbuild_args)

        self.assertEqual('test.signing.bundle_id',
                         _get_adjacent_item(productbuild_args, '--identifier'))
        self.assertEqual('99.0.9999.99',
                         _get_adjacent_item(productbuild_args, '--version'))
        self.assertEqual(
            '/$W_1/App Product.dist',
            _get_adjacent_item(productbuild_args, '--distribution'))
        self.assertEqual(
            '/$W_1', _get_adjacent_item(productbuild_args, '--package-path'))
        self.assertEqual('[INSTALLER-IDENTITY]',
                         _get_adjacent_item(productbuild_args, '--sign'))

    @mock.patch('signing.pipeline._component_property_path',
                _component_property_path)
    @mock.patch('signing.pipeline._productbuild_distribution_path',
                _productbuild_distribution_path)
    @mock.patch('signing.pipeline._create_pkgbuild_scripts',
                _create_pkgbuild_scripts)
    @mock.patch('signing.commands.macos_version', _macos_version_12)
    @mock.patch('signing.pipeline._minimum_os_version', _minimum_os_version)
    def test_package_and_sign_pkg_branding_12(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig()
        dist = model.Distribution(
            branding_code='MOO',
            packaging_name_fragment='ForCows',
            package_as_dmg=False,
            package_as_pkg=True,
            package_as_zip=False)
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual('/$O/AppProduct-99.0.9999.99-ForCows.pkg',
                         pipeline._package_and_sign_pkg(paths, dist_config))

        manager.assert_has_calls(
            [mock.call.run_command(mock.ANY),
             mock.call.run_command(mock.ANY)])

        run_commands = [
            call for call in manager.mock_calls if call[0] == 'run_command'
        ]
        pkgbuild_args = run_commands[0][1][0]
        productbuild_args = run_commands[1][1][0]

        self.assertEqual('/$W_1/payload',
                         _get_adjacent_item(pkgbuild_args, '--root'))
        self.assertEqual('/$W_1/App Product.plist',
                         _get_adjacent_item(pkgbuild_args, '--component-plist'))
        self.assertEqual('test.signing.bundle_id',
                         _get_adjacent_item(pkgbuild_args, '--identifier'))
        self.assertEqual('99.0.9999.99',
                         _get_adjacent_item(pkgbuild_args, '--version'))
        self.assertEqual('/$W_1/scripts',
                         _get_adjacent_item(pkgbuild_args, '--scripts'))

        self.assertEqual('latest',
                         _get_adjacent_item(pkgbuild_args, '--compression'))
        self.assertEqual('10.11.0',
                         _get_adjacent_item(pkgbuild_args, '--min-os-version'))

        self.assertEqual('test.signing.bundle_id',
                         _get_adjacent_item(productbuild_args, '--identifier'))
        self.assertEqual('99.0.9999.99',
                         _get_adjacent_item(productbuild_args, '--version'))
        self.assertEqual(
            '/$W_1/App Product.dist',
            _get_adjacent_item(productbuild_args, '--distribution'))
        self.assertEqual(
            '/$W_1', _get_adjacent_item(productbuild_args, '--package-path'))
        self.assertEqual('[INSTALLER-IDENTITY]',
                         _get_adjacent_item(productbuild_args, '--sign'))

    def test_package_dmg_no_customize(self, **kwargs):
        dist = model.Distribution()
        config = test_config.TestConfig()
        paths = self.paths.replace_work('/$W')

        dmg_path = pipeline._package_dmg(paths, dist, config)
        self.assertEqual('/$O/AppProduct-99.0.9999.99.dmg', dmg_path)

        pkg_dmg_args = kwargs['run_command'].mock_calls[0][1][0]

        self.assertEqual(dmg_path, _get_adjacent_item(pkg_dmg_args, '--target'))
        self.assertEqual('/$I/Product Packaging/chrome_dmg_icon.icns',
                         _get_adjacent_item(pkg_dmg_args, '--icon'))
        self.assertEqual('App Product',
                         _get_adjacent_item(pkg_dmg_args, '--volname'))
        self.assertEqual('/$W/empty',
                         _get_adjacent_item(pkg_dmg_args, '--source'))

        copy_specs = [
            pkg_dmg_args[i + 1]
            for i, arg in enumerate(pkg_dmg_args)
            if arg == '--copy'
        ]
        self.assertEqual(
            set(copy_specs),
            set([
                '/$W/App Product.app:/',
                '/$I/Product Packaging/keystone_install.sh:/.keystone_install',
                '/$I/Product Packaging/chrome_dmg_background.png:/.background/background.png',
                '/$I/Product Packaging/chrome_dmg_dsstore:/.DS_Store'
            ]))

    def test_package_dmg_customize(self, **kwargs):
        dist = model.Distribution(
            channel_customize=True,
            channel='canary',
            app_name_fragment='Canary',
            product_dirname='canary',
            creator_code='cana')
        config = dist.to_config(test_config.TestConfig())
        paths = self.paths.replace_work('/$W')

        dmg_path = pipeline._package_dmg(paths, dist, config)
        self.assertEqual('/$O/AppProductCanary-99.0.9999.99.dmg', dmg_path)

        pkg_dmg_args = kwargs['run_command'].mock_calls[0][1][0]

        self.assertEqual(dmg_path, _get_adjacent_item(pkg_dmg_args, '--target'))
        self.assertEqual('/$I/Product Packaging/chrome_canary_dmg_icon.icns',
                         _get_adjacent_item(pkg_dmg_args, '--icon'))
        self.assertEqual('App Product Canary',
                         _get_adjacent_item(pkg_dmg_args, '--volname'))
        self.assertEqual('/$W/empty',
                         _get_adjacent_item(pkg_dmg_args, '--source'))

        copy_specs = [
            pkg_dmg_args[i + 1]
            for i, arg in enumerate(pkg_dmg_args)
            if arg == '--copy'
        ]
        self.assertEqual(
            set(copy_specs),
            set([
                '/$W/App Product Canary.app:/',
                '/$I/Product Packaging/keystone_install.sh:/.keystone_install',
                '/$I/Product Packaging/chrome_dmg_background.png:/.background/background.png',
                '/$I/Product Packaging/chrome_canary_dmg_dsstore:/.DS_Store'
            ]))

    def test_package_dmg_no_customize_not_chrome(self, **kwargs):
        dist = model.Distribution()
        config = test_config.TestConfigNonChromeBranded()
        paths = self.paths.replace_work('/$W')

        dmg_path = pipeline._package_dmg(paths, dist, config)
        self.assertEqual('/$O/AppProduct-99.0.9999.99.dmg', dmg_path)

        pkg_dmg_args = kwargs['run_command'].mock_calls[0][1][0]

        self.assertEqual(dmg_path, _get_adjacent_item(pkg_dmg_args, '--target'))
        self.assertEqual('App Product',
                         _get_adjacent_item(pkg_dmg_args, '--volname'))
        self.assertEqual('/$W/empty',
                         _get_adjacent_item(pkg_dmg_args, '--source'))

        copy_specs = [
            pkg_dmg_args[i + 1]
            for i, arg in enumerate(pkg_dmg_args)
            if arg == '--copy'
        ]
        self.assertEqual(set(copy_specs), set(['/$W/App Product.app:/']))

    def test_package_installer_tools(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig()
        pipeline._package_installer_tools(self.paths, config)

        # Start and end with the work dir.
        self.assertEqual(
            mock.call.make_dir('/$W_1/diff_tools'), manager.mock_calls[0])
        self.assertEqual(
            mock.call.shutil.rmtree('/$W_1'), manager.mock_calls[-1])

        self.assertEqual(
            mock.call.run_command(
                ['zip', '-9ry', '/$O/diff_tools.zip', 'diff_tools'],
                cwd='/$W_1'), manager.mock_calls[-2])

        files_to_copy = set([
            'goobspatch',
            'liblzma_decompress.dylib',
            'goobsdiff',
            'xz',
            'xzdec',
            'dirdiffer.sh',
            'dirpatcher.sh',
            'dmgdiffer.sh',
            'keystone_install.sh',
            'pkg-dmg',
        ])
        copied_files = []
        for call in manager.mock_calls:
            if call[0] == 'copy_files':
                args = call[1]
                self.assertTrue(args[0].startswith('/$I/Product Packaging/'))
                self.assertEqual('/$W_1/diff_tools', args[1])
                copied_files.append(os.path.basename(args[0]))

        self.assertEqual(len(copied_files), len(files_to_copy))
        self.assertEqual(set(copied_files), files_to_copy)

        files_to_sign = set([
            'goobspatch',
            'liblzma_decompress.dylib',
            'goobsdiff',
            'xz',
            'xzdec',
        ])
        signed_files = []
        verified_files = []

        for call in manager.mock_calls:
            args = call[1]
            if call[0] == 'sign_part':
                signed_files.append(os.path.basename(args[2].path))
            elif call[0] == 'verify_part':
                path = os.path.basename(args[1].path)
                self.assertTrue(path in signed_files)
                verified_files.append(path)

        self.assertEqual(len(signed_files), len(files_to_sign))
        self.assertEqual(len(verified_files), len(files_to_sign))
        self.assertEqual(set(signed_files), files_to_sign)
        self.assertEqual(set(verified_files), files_to_sign)

    def test_package_installer_tools_not_chrome(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfigNonChromeBranded()
        pipeline._package_installer_tools(self.paths, config)

        files_to_copy = set([
            'goobspatch',
            'liblzma_decompress.dylib',
            'goobsdiff',
            'xz',
            'xzdec',
            'dirdiffer.sh',
            'dirpatcher.sh',
            'dmgdiffer.sh',
            'pkg-dmg',
        ])
        copied_files = []
        for call in manager.mock_calls:
            if call[0] == 'copy_files':
                args = call[1]
                self.assertTrue(args[0].startswith('/$I/Product Packaging/'))
                self.assertEqual('/$W_1/diff_tools', args[1])
                copied_files.append(os.path.basename(args[0]))

        self.assertEqual(len(copied_files), len(files_to_copy))
        self.assertEqual(set(copied_files), files_to_copy)

    def test_filter_distributions(self, **kwargs):
        dist1 = model.Distribution()
        dist2 = model.Distribution(branding_code='MOO', channel='beta')
        dist3 = model.Distribution(branding_code='ARF', channel='dev')
        dist4 = model.Distribution(branding_code='MOOF', channel='canary')

        distributions = [dist1, dist2, dist3, dist4]

        # --- Neither ---

        # No filters should yield no change to the distribution list.
        self.assertEqual(distributions,
                         pipeline._filter_distributions(distributions, [], []))

        # --- Brands only ---

        # Filtering a brand code not being built should throw.
        with self.assertRaises(ValueError) as cm:
            pipeline._filter_distributions(distributions, ['MOOG'], [])
        self.assertEqual(
            cm.exception.args[0],
            "Brand codes do not match any distribution: %r" % {'MOOG'})

        # Filtering one or more brand codes explicitly should remove them.
        self.assertEqual([dist1, dist2, dist3],
                         pipeline._filter_distributions(distributions, ['MOOF'],
                                                        []))
        self.assertEqual([dist1, dist3],
                         pipeline._filter_distributions(distributions,
                                                        ['MOO', 'MOOF'], []))

        # Filtering a '*' should remove all brand coded distributions.
        self.assertEqual([dist1],
                         pipeline._filter_distributions(distributions, ['*'],
                                                        []))

        # Filtering a specific brand code and '*' should remove all brand coded
        # distributions.
        self.assertEqual([dist1],
                         pipeline._filter_distributions(distributions,
                                                        ['*', 'MOOF'], []))

        # Filtering all brand codes when there aren't any should yield no change
        # to the distribution list.
        self.assertEqual([dist1],
                         pipeline._filter_distributions([dist1], ['*'], []))

        # --- Channels ---

        # Filtering for a channel not being built should throw.
        with self.assertRaises(ValueError) as cm:
            pipeline._filter_distributions(distributions, [], ['hyper'])
        self.assertEqual(
            cm.exception.args[0],
            "Channels do not match any distribution: %r" % {'hyper'})

        # Filtering for 'stable' should result in the distribution with None
        # as a channel.
        self.assertEqual([dist1],
                         pipeline._filter_distributions(distributions, [],
                                                        ['stable']))

        # Filtering for any other string as channel name should work.
        self.assertEqual([dist2],
                         pipeline._filter_distributions(distributions, [],
                                                        ['beta']))

        # Filtering for 'stable' along with other strings should work.
        self.assertEqual([dist1, dist3],
                         pipeline._filter_distributions(distributions, [],
                                                        ['stable', 'dev']))

        # --- Both ---

        # Filtering on both in a way that allows a result should work.
        self.assertEqual([dist2],
                         pipeline._filter_distributions(distributions, ['MOOF'],
                                                        ['beta']))

        # Filtering for inclusion of a channel that is filtered out due to brand
        # code should throw.
        with self.assertRaises(ValueError) as cm:
            pipeline._filter_distributions(distributions, ['MOO'], ['beta'])
        self.assertEqual(
            cm.exception.args[0],
            "All distributions for channels were filtered out by brand: %r" %
            {'beta'})


@mock.patch.multiple(
    'signing.commands', **{
        m: mock.DEFAULT for m in ('move_file', 'copy_files', 'run_command',
                                  'make_dir', 'shutil', 'os')
    })
@mock.patch.multiple('signing.notarize', **{
    m: mock.DEFAULT for m in ('submit', 'wait_for_results', 'staple')
})
@mock.patch.multiple(
    'signing.pipeline', **{
        m: mock.DEFAULT
        for m in ('_customize_and_sign_chrome', '_staple_chrome',
                  '_package_and_sign_dmg', '_package_and_sign_pkg',
                  '_package_zip', '_package_installer_tools')
    })
@mock.patch('signing.commands.tempfile.mkdtemp', _get_work_dir)
class TestSignAll(unittest.TestCase):

    def setUp(self, **kwargs):
        _get_work_dir.count = 0
        self.paths = model.Paths('/$I', '/$O', None)

    def test_sign_basic_distribution_dmg(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        app_uuid = 'f38ee49c-c55b-4a10-a4f5-aaaa17636b76'
        dmg_uuid = '9f49067e-a13d-436a-8016-3a22a4f6ef92'
        kwargs['submit'].side_effect = [app_uuid, dmg_uuid]
        kwargs['wait_for_results'].side_effect = [
            iter([app_uuid]), iter([dmg_uuid])
        ]
        kwargs[
            '_package_and_sign_dmg'].return_value = '/$O/AppProduct-99.0.9999.99.dmg'

        class Config(test_config.TestConfig):

            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=True,
                        package_as_pkg=False,
                        package_as_zip=False),
                ]

        config = Config()
        pipeline.sign_all(self.paths, config)

        self.assertEqual(1, kwargs['_package_installer_tools'].call_count)

        manager.assert_has_calls([
            # First customize the distribution and sign it.
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable', mock.ANY),

            # Prepare the app for notarization.
            mock.call.run_command([
                'zip', '--recurse-paths', '--symlinks', '--quiet',
                '/$W_1/AppProduct-99.0.9999.99.zip', 'App Product.app'
            ],
                                  cwd='/$W_1/stable'),
            mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
            mock.call.shutil.rmtree('/$W_2'),
            mock.call.wait_for_results({app_uuid: None}.keys(), mock.ANY),
            mock.call._staple_chrome(
                self.paths.replace_work('/$W_1/stable'), mock.ANY),

            # Make the DMG.
            mock.call._package_and_sign_dmg(mock.ANY, mock.ANY),

            # Notarize the DMG.
            mock.call.submit('/$O/AppProduct-99.0.9999.99.dmg', mock.ANY),
            mock.call.wait_for_results({dmg_uuid: None}.keys(), mock.ANY),
            mock.call.staple('/$O/AppProduct-99.0.9999.99.dmg'),
            mock.call.shutil.rmtree('/$W_1'),

            # Package the installer tools.
            mock.call._package_installer_tools(mock.ANY, mock.ANY),
        ])

    def test_sign_basic_distribution_zip(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        app_uuid = 'f38ee49c-c55b-4a10-a4f5-aaaa17636b76'
        kwargs['submit'].side_effect = [app_uuid]
        kwargs['wait_for_results'].side_effect = [
            iter([app_uuid]),
            iter([]),
        ]
        kwargs['_package_zip'].return_value = '/$O/AppProduct-99.0.9999.99.zip'

        class Config(test_config.TestConfig):

            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=False,
                        package_as_pkg=False,
                        package_as_zip=True),
                ]

        config = Config()
        pipeline.sign_all(self.paths, config)

        self.assertEqual(1, kwargs['_package_installer_tools'].call_count)

        manager.assert_has_calls([
            # First customize the distribution and sign it.
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable', mock.ANY),

            # Prepare the app for notarization.
            mock.call.run_command([
                'zip', '--recurse-paths', '--symlinks', '--quiet',
                '/$W_1/AppProduct-99.0.9999.99.zip', 'App Product.app'
            ],
                                  cwd='/$W_1/stable'),
            mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
            mock.call.shutil.rmtree('/$W_2'),
            mock.call.wait_for_results({app_uuid: None}.keys(), mock.ANY),
            mock.call._staple_chrome(
                self.paths.replace_work('/$W_1/stable'), mock.ANY),

            # Make the ZIP.
            mock.call._package_zip(mock.ANY, mock.ANY),

            # Notarize the DMG.
            mock.call.wait_for_results({}.keys(), mock.ANY),
            mock.call.shutil.rmtree('/$W_1'),

            # Package the installer tools.
            mock.call._package_installer_tools(mock.ANY, mock.ANY),
        ])

    def test_sign_inflated_distribution_dmg(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        app_uuid = 'f38ee49c-c55b-4a10-a4f5-aaaa17636b76'
        dmg_uuid = '9f49067e-a13d-436a-8016-3a22a4f6ef92'
        kwargs['submit'].side_effect = [app_uuid, dmg_uuid]
        kwargs['wait_for_results'].side_effect = [
            iter([app_uuid]), iter([dmg_uuid])
        ]
        kwargs[
            '_package_and_sign_dmg'].return_value = '/$O/AppProduct-99.0.9999.99.dmg'

        class Config(test_config.TestConfig):

            @property
            def distributions(self):
                return [
                    model.Distribution(
                        inflation_kilobytes=5000,
                        package_as_dmg=True,
                        package_as_pkg=False,
                        package_as_zip=False),
                ]

        config = Config()

        pipeline.sign_all(self.paths, config)

        self.assertEqual(1, kwargs['_package_installer_tools'].call_count)

        manager.assert_has_calls([
            # First customize the distribution and sign it.
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable-5000', mock.ANY),

            # Prepare the app for notarization.
            mock.call.run_command([
                'zip', '--recurse-paths', '--symlinks', '--quiet',
                '/$W_1/AppProduct-99.0.9999.99.zip', 'App Product.app'
            ],
                                  cwd='/$W_1/stable-5000'),
            mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
            mock.call.shutil.rmtree('/$W_2'),
            mock.call.wait_for_results({app_uuid: None}.keys(), mock.ANY),
            mock.call._staple_chrome(
                self.paths.replace_work('/$W_1/stable-5000'), mock.ANY),
            mock.call.run_command([
                'dd', 'if=/dev/urandom',
                'of=/$I/Product Packaging/inflation.bin', 'bs=1000',
                'count=5000'
            ]),

            # Make the DMG.
            mock.call._package_and_sign_dmg(mock.ANY, mock.ANY),

            # Notarize the DMG.
            mock.call.submit('/$O/AppProduct-99.0.9999.99.dmg', mock.ANY),
            mock.call.wait_for_results({dmg_uuid: None}.keys(), mock.ANY),
            mock.call.staple('/$O/AppProduct-99.0.9999.99.dmg'),
            mock.call.shutil.rmtree('/$W_1'),

            # Package the installer tools.
            mock.call._package_installer_tools(mock.ANY, mock.ANY),
        ])

    def test_sign_basic_distribution_pkg(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        app_uuid = 'b2ce64e5-4fae-4043-9c20-d9ff53065b2a'
        pkg_uuid = 'cb811baf-5d35-4caa-adf4-1f61b4991eed'
        kwargs['submit'].side_effect = [app_uuid, pkg_uuid]
        kwargs['wait_for_results'].side_effect = [
            iter([app_uuid]), iter([pkg_uuid])
        ]
        kwargs[
            '_package_and_sign_pkg'].return_value = '/$O/AppProduct-99.0.9999.99.pkg'

        class Config(test_config.TestConfig):

            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=False,
                        package_as_pkg=True,
                        package_as_zip=False),
                ]

        config = Config()
        pipeline.sign_all(self.paths, config)

        self.assertEqual(1, kwargs['_package_installer_tools'].call_count)

        manager.assert_has_calls([
            # First customize the distribution and sign it.
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable', mock.ANY),

            # Prepare the app for notarization.
            mock.call.run_command([
                'zip', '--recurse-paths', '--symlinks', '--quiet',
                '/$W_1/AppProduct-99.0.9999.99.zip', 'App Product.app'
            ],
                                  cwd='/$W_1/stable'),
            mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
            mock.call.shutil.rmtree('/$W_2'),
            mock.call.wait_for_results({app_uuid: None}.keys(), mock.ANY),
            mock.call._staple_chrome(
                self.paths.replace_work('/$W_1/stable'), mock.ANY),

            # Make the DMG.
            mock.call._package_and_sign_pkg(mock.ANY, mock.ANY),

            # Notarize the DMG.
            mock.call.submit('/$O/AppProduct-99.0.9999.99.pkg', mock.ANY),
            mock.call.wait_for_results({pkg_uuid: None}.keys(), mock.ANY),
            mock.call.staple('/$O/AppProduct-99.0.9999.99.pkg'),
            mock.call.shutil.rmtree('/$W_1'),

            # Package the installer tools.
            mock.call._package_installer_tools(mock.ANY, mock.ANY),
        ])

    def test_sign_basic_distribution_dmg_zip(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        app_uuid = '6de7df90-cf07-4213-9ce6-45f83588a386'
        dmg_uuid = '7f77eefd-6c9d-4271-9367-760dc78a49dd'
        kwargs['submit'].side_effect = [app_uuid, dmg_uuid]
        kwargs['wait_for_results'].side_effect = [
            iter([app_uuid]), iter([dmg_uuid])
        ]
        kwargs[
            '_package_and_sign_dmg'].return_value = '/$O/AppProduct-99.0.9999.99.dmg'
        kwargs['_package_zip'].return_value = '/$O/AppProduct-99.0.9999.99.zip'

        class Config(test_config.TestConfig):

            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=True,
                        package_as_pkg=False,
                        package_as_zip=True),
                ]

        config = Config()
        pipeline.sign_all(self.paths, config)

        self.assertEqual(1, kwargs['_package_installer_tools'].call_count)

        manager.assert_has_calls([
            # First customize the distribution and sign it.
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable', mock.ANY),

            # Prepare the app for notarization.
            mock.call.run_command([
                'zip', '--recurse-paths', '--symlinks', '--quiet',
                '/$W_1/AppProduct-99.0.9999.99.zip', 'App Product.app'
            ],
                                  cwd='/$W_1/stable'),
            mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
            mock.call.shutil.rmtree('/$W_2'),
            mock.call.wait_for_results({app_uuid: None}.keys(), mock.ANY),
            mock.call._staple_chrome(
                self.paths.replace_work('/$W_1/stable'), mock.ANY),

            # Make the DMG.
            mock.call._package_and_sign_dmg(mock.ANY, mock.ANY),
            mock.call.submit('/$O/AppProduct-99.0.9999.99.dmg', mock.ANY),

            # Make the ZIP.
            mock.call._package_zip(mock.ANY, mock.ANY),

            # Notarize the DMG.
            mock.call.wait_for_results({dmg_uuid: None}.keys(), mock.ANY),
            mock.call.staple('/$O/AppProduct-99.0.9999.99.dmg'),
            mock.call.shutil.rmtree('/$W_1'),

            # Package the installer tools.
            mock.call._package_installer_tools(mock.ANY, mock.ANY),
        ])

    def test_sign_basic_distribution_dmg_pkg(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        app_uuid = '6de7df90-cf07-4213-9ce6-45f83588a386'
        dmg_uuid = '7f77eefd-6c9d-4271-9367-760dc78a49dd'
        pkg_uuid = '364d9b29-a0a0-4661-b366-e35449197671'
        kwargs['submit'].side_effect = [app_uuid, dmg_uuid, pkg_uuid]
        kwargs['wait_for_results'].side_effect = [
            iter([app_uuid]), iter([dmg_uuid, pkg_uuid])
        ]
        kwargs[
            '_package_and_sign_dmg'].return_value = '/$O/AppProduct-99.0.9999.99.dmg'
        kwargs[
            '_package_and_sign_pkg'].return_value = '/$O/AppProduct-99.0.9999.99.pkg'

        class Config(test_config.TestConfig):

            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=True,
                        package_as_pkg=True,
                        package_as_zip=False),
                ]

        config = Config()
        pipeline.sign_all(self.paths, config)

        self.assertEqual(1, kwargs['_package_installer_tools'].call_count)

        manager.assert_has_calls([
            # First customize the distribution and sign it.
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable', mock.ANY),

            # Prepare the app for notarization.
            mock.call.run_command([
                'zip', '--recurse-paths', '--symlinks', '--quiet',
                '/$W_1/AppProduct-99.0.9999.99.zip', 'App Product.app'
            ],
                                  cwd='/$W_1/stable'),
            mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
            mock.call.shutil.rmtree('/$W_2'),
            mock.call.wait_for_results({app_uuid: None}.keys(), mock.ANY),
            mock.call._staple_chrome(
                self.paths.replace_work('/$W_1/stable'), mock.ANY),

            # Make the DMG, and submit for notarization.
            mock.call._package_and_sign_dmg(mock.ANY, mock.ANY),
            mock.call.submit('/$O/AppProduct-99.0.9999.99.dmg', mock.ANY),

            # Make the PKG, and submit for notarization.
            mock.call._package_and_sign_pkg(mock.ANY, mock.ANY),
            mock.call.submit('/$O/AppProduct-99.0.9999.99.pkg', mock.ANY),

            # Wait for notarization results.
            mock.call.wait_for_results({
                dmg_uuid: None,
                pkg_uuid: None
            }.keys(), mock.ANY),
            mock.call.staple('/$O/AppProduct-99.0.9999.99.dmg'),
            mock.call.staple('/$O/AppProduct-99.0.9999.99.pkg'),
            mock.call.shutil.rmtree('/$W_1'),

            # Package the installer tools.
            mock.call._package_installer_tools(mock.ANY, mock.ANY),
        ])

    def test_sign_basic_distribution_pkg_zip(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        app_uuid = '6de7df90-cf07-4213-9ce6-45f83588a386'
        pkg_uuid = '364d9b29-a0a0-4661-b366-e35449197671'
        kwargs['submit'].side_effect = [app_uuid, pkg_uuid]
        kwargs['wait_for_results'].side_effect = [
            iter([app_uuid]), iter([pkg_uuid])
        ]
        kwargs[
            '_package_and_sign_pkg'].return_value = '/$O/AppProduct-99.0.9999.99.pkg'
        kwargs['_package_zip'].return_value = '/$O/AppProduct-99.0.9999.99.zip'

        class Config(test_config.TestConfig):

            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=False,
                        package_as_pkg=True,
                        package_as_zip=True),
                ]

        config = Config()
        pipeline.sign_all(self.paths, config)

        self.assertEqual(1, kwargs['_package_installer_tools'].call_count)

        manager.assert_has_calls([
            # First customize the distribution and sign it.
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable', mock.ANY),

            # Prepare the app for notarization.
            mock.call.run_command([
                'zip', '--recurse-paths', '--symlinks', '--quiet',
                '/$W_1/AppProduct-99.0.9999.99.zip', 'App Product.app'
            ],
                                  cwd='/$W_1/stable'),
            mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
            mock.call.shutil.rmtree('/$W_2'),
            mock.call.wait_for_results({app_uuid: None}.keys(), mock.ANY),
            mock.call._staple_chrome(
                self.paths.replace_work('/$W_1/stable'), mock.ANY),

            # Make the PKG, and submit for notarization.
            mock.call._package_and_sign_pkg(mock.ANY, mock.ANY),
            mock.call.submit('/$O/AppProduct-99.0.9999.99.pkg', mock.ANY),

            # Make the ZIP.
            mock.call._package_zip(mock.ANY, mock.ANY),

            # Notarize the PKG.
            mock.call.wait_for_results({pkg_uuid: None}.keys(), mock.ANY),
            mock.call.staple('/$O/AppProduct-99.0.9999.99.pkg'),
            mock.call.shutil.rmtree('/$W_1'),

            # Package the installer tools.
            mock.call._package_installer_tools(mock.ANY, mock.ANY),
        ])

    def test_sign_basic_distribution_dmg_pkg_zip(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        app_uuid = '6de7df90-cf07-4213-9ce6-45f83588a386'
        dmg_uuid = '7f77eefd-6c9d-4271-9367-760dc78a49dd'
        pkg_uuid = '364d9b29-a0a0-4661-b366-e35449197671'
        kwargs['submit'].side_effect = [app_uuid, dmg_uuid, pkg_uuid]
        kwargs['wait_for_results'].side_effect = [
            iter([app_uuid]), iter([dmg_uuid, pkg_uuid])
        ]
        kwargs[
            '_package_and_sign_dmg'].return_value = '/$O/AppProduct-99.0.9999.99.dmg'
        kwargs[
            '_package_and_sign_pkg'].return_value = '/$O/AppProduct-99.0.9999.99.pkg'
        kwargs['_package_zip'].return_value = '/$O/AppProduct-99.0.9999.99.zip'

        class Config(test_config.TestConfig):

            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=True,
                        package_as_pkg=True,
                        package_as_zip=True),
                ]

        config = Config()
        pipeline.sign_all(self.paths, config)

        self.assertEqual(1, kwargs['_package_installer_tools'].call_count)

        manager.assert_has_calls([
            # First customize the distribution and sign it.
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable', mock.ANY),

            # Prepare the app for notarization.
            mock.call.run_command([
                'zip', '--recurse-paths', '--symlinks', '--quiet',
                '/$W_1/AppProduct-99.0.9999.99.zip', 'App Product.app'
            ],
                                  cwd='/$W_1/stable'),
            mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
            mock.call.shutil.rmtree('/$W_2'),
            mock.call.wait_for_results({app_uuid: None}.keys(), mock.ANY),
            mock.call._staple_chrome(
                self.paths.replace_work('/$W_1/stable'), mock.ANY),

            # Make the DMG.
            mock.call._package_and_sign_dmg(mock.ANY, mock.ANY),
            mock.call.submit('/$O/AppProduct-99.0.9999.99.dmg', mock.ANY),

            # Make the PKG, and submit for notarization.
            mock.call._package_and_sign_pkg(mock.ANY, mock.ANY),
            mock.call.submit('/$O/AppProduct-99.0.9999.99.pkg', mock.ANY),

            # Make the ZIP.
            mock.call._package_zip(mock.ANY, mock.ANY),

            # Notarize the DMG.
            mock.call.wait_for_results({
                dmg_uuid: None,
                pkg_uuid: None
            }.keys(), mock.ANY),
            mock.call.staple('/$O/AppProduct-99.0.9999.99.dmg'),
            mock.call.staple('/$O/AppProduct-99.0.9999.99.pkg'),
            mock.call.shutil.rmtree('/$W_1'),

            # Package the installer tools.
            mock.call._package_installer_tools(mock.ANY, mock.ANY),
        ])

    def test_sign_no_packaging(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        app_uuid = '2d7bf857-c2b2-4ebc-8b51-b2f43ecfc13e'
        kwargs['submit'].return_value = app_uuid
        kwargs['wait_for_results'].return_value = iter([app_uuid])

        config = test_config.TestConfig()
        pipeline.sign_all(self.paths, config, disable_packaging=True)

        manager.assert_has_calls([
            # First customize the distribution and sign it.
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable', mock.ANY),

            # Prepare the app for notarization.
            mock.call.run_command([
                'zip', '--recurse-paths', '--symlinks', '--quiet',
                '/$W_1/AppProduct-99.0.9999.99.zip', 'App Product.app'
            ],
                                  cwd='/$W_1/stable'),
            mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
            mock.call.shutil.rmtree('/$W_2'),
            mock.call.wait_for_results({app_uuid: None}.keys(), mock.ANY),
            mock.call._staple_chrome(
                self.paths.replace_work('/$W_1/stable'), mock.ANY),
            mock.call.shutil.rmtree('/$W_1'),

            # Package the installer tools.
            mock.call._package_installer_tools(mock.ANY, mock.ANY),
        ])

        self.assertEqual(1, kwargs['_package_installer_tools'].call_count)
        self.assertEqual(1, kwargs['run_command'].call_count)

    def test_sign_notarize_no_wait(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        app_uuid = 'f38ee49c-c55b-4a10-a4f5-aaaa17636b76'
        dmg_uuid = '9f49067e-a13d-436a-8016-3a22a4f6ef92'
        kwargs['submit'].side_effect = [app_uuid, dmg_uuid]
        kwargs['wait_for_results'].side_effect = [
            iter([app_uuid]), iter([dmg_uuid])
        ]
        kwargs['_package_and_sign_dmg'].return_value = (
            '/$O/AppProduct-99.0.9999.99.dmg')

        config = test_config.TestConfig(
            notarize=model.NotarizeAndStapleLevel.NOWAIT)
        pipeline.sign_all(self.paths, config)

        self.assertEqual(1, kwargs['_package_installer_tools'].call_count)

        manager.assert_has_calls([
            # First customize the distribution and sign it.
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable', mock.ANY),

            # Prepare the app for notarization.
            mock.call.run_command([
                'zip', '--recurse-paths', '--symlinks', '--quiet',
                '/$W_1/AppProduct-99.0.9999.99.zip', 'App Product.app'
            ],
                                  cwd='/$W_1/stable'),
            mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
            mock.call.shutil.rmtree('/$W_2'),

            # Make the DMG.
            mock.call._package_and_sign_dmg(mock.ANY, mock.ANY),

            # Notarize the DMG.
            mock.call.submit('/$O/AppProduct-99.0.9999.99.dmg', mock.ANY),
            mock.call.shutil.rmtree('/$W_1'),

            # Package the installer tools.
            mock.call._package_installer_tools(mock.ANY, mock.ANY),
        ])

    def test_sign_notarize_wait_no_staple(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        app_uuid = 'f38ee49c-c55b-4a10-a4f5-aaaa17636b76'
        dmg_uuid = '9f49067e-a13d-436a-8016-3a22a4f6ef92'
        kwargs['submit'].side_effect = [app_uuid, dmg_uuid]
        kwargs['wait_for_results'].side_effect = [
            iter([app_uuid]), iter([dmg_uuid])
        ]
        kwargs[
            '_package_and_sign_dmg'].return_value = '/$O/AppProduct-99.0.9999.99.dmg'

        config = test_config.TestConfig(
            notarize=model.NotarizeAndStapleLevel.WAIT_NOSTAPLE)
        pipeline.sign_all(self.paths, config)

        self.assertEqual(1, kwargs['_package_installer_tools'].call_count)

        manager.assert_has_calls([
            # First customize the distribution and sign it.
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable', mock.ANY),

            # Prepare the app for notarization.
            mock.call.run_command([
                'zip', '--recurse-paths', '--symlinks', '--quiet',
                '/$W_1/AppProduct-99.0.9999.99.zip', 'App Product.app'
            ],
                                  cwd='/$W_1/stable'),
            mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
            mock.call.shutil.rmtree('/$W_2'),
            mock.call.wait_for_results({app_uuid: None}.keys(), mock.ANY),

            # Make the DMG.
            mock.call._package_and_sign_dmg(mock.ANY, mock.ANY),

            # Notarize the DMG.
            mock.call.submit('/$O/AppProduct-99.0.9999.99.dmg', mock.ANY),

            # Cleanup.
            mock.call.wait_for_results({dmg_uuid: None}.keys(), mock.ANY),
            mock.call.shutil.rmtree('/$W_1'),

            # Package the installer tools.
            mock.call._package_installer_tools(mock.ANY, mock.ANY),
        ])

    def test_sign_no_notarization(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig(
            notarize=model.NotarizeAndStapleLevel.NONE)
        pipeline.sign_all(self.paths, config)

        self.assertEqual(1, kwargs['_package_installer_tools'].call_count)

        manager.assert_has_calls([
            # First customize the distribution and sign it.
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable', mock.ANY),
            mock.call.shutil.rmtree('/$W_2'),

            # Make the DMG.
            mock.call._package_and_sign_dmg(mock.ANY, mock.ANY),

            # Cleanup.
            mock.call.shutil.rmtree('/$W_1'),

            # Package the installer tools.
            mock.call._package_installer_tools(mock.ANY, mock.ANY),
        ])

    def test_sign_no_packaging_no_notarization(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig(
            notarize=model.NotarizeAndStapleLevel.NONE)
        pipeline.sign_all(self.paths, config, disable_packaging=True)

        manager.assert_has_calls([
            # First customize the distribution and sign it.
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$O/stable', mock.ANY),
            mock.call.shutil.rmtree('/$W_2'),
            mock.call.shutil.rmtree('/$W_1'),

            # Package the installer tools.
            mock.call._package_installer_tools(mock.ANY, mock.ANY),
        ])

        self.assertEqual(1, kwargs['_package_installer_tools'].call_count)
        self.assertEqual(0, kwargs['run_command'].call_count)

    def test_sign_branded_distribution(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        class Config(test_config.TestConfig):

            @property
            def distributions(self):
                return [
                    model.Distribution(),
                    model.Distribution(
                        branding_code='MOO',
                        packaging_name_fragment='ForCows',
                        package_as_dmg=True,
                        package_as_pkg=False,
                        package_as_zip=False),
                    model.Distribution(
                        branding_code='ARF',
                        packaging_name_fragment='ForDogs',
                        package_as_dmg=False,
                        package_as_pkg=True,
                        package_as_zip=False),
                    model.Distribution(
                        branding_code='MEOW',
                        packaging_name_fragment='ForCats',
                        package_as_dmg=False,
                        package_as_pkg=False,
                        package_as_zip=True),
                    model.Distribution(
                        branding_code='MOOF',
                        packaging_name_fragment='ForDogcows',
                        package_as_dmg=True,
                        package_as_pkg=True,
                        package_as_zip=False),
                    model.Distribution(
                        branding_code='MEOARF',
                        packaging_name_fragment='ForCatdogs',
                        package_as_dmg=False,
                        package_as_pkg=True,
                        package_as_zip=True),
                    model.Distribution(
                        branding_code='MOOEOW',
                        packaging_name_fragment='ForCowcats',
                        package_as_dmg=True,
                        package_as_pkg=False,
                        package_as_zip=True),
                    model.Distribution(
                        branding_code='AHHHH',
                        packaging_name_fragment='ForCowdogcats',
                        package_as_dmg=True,
                        package_as_pkg=True,
                        package_as_zip=True),
                ]

        config = Config(notarize=model.NotarizeAndStapleLevel.NONE)
        pipeline.sign_all(self.paths, config)

        self.assertEqual(1, kwargs['_package_installer_tools'].call_count)
        self.assertEqual(6, kwargs['_customize_and_sign_chrome'].call_count)

        manager.assert_has_calls([
            # Customizations.
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable', mock.ANY),
            mock.call.shutil.rmtree('/$W_2'),
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable-MOO', mock.ANY),
            mock.call.shutil.rmtree('/$W_3'),
            mock.call.shutil.rmtree('/$W_4'),
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable-MEOW', mock.ANY),
            mock.call.shutil.rmtree('/$W_5'),
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable-MOOF', mock.ANY),
            mock.call.shutil.rmtree('/$W_6'),
            mock.call.shutil.rmtree('/$W_7'),
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable-MOOEOW',
                                                 mock.ANY),
            mock.call.shutil.rmtree('/$W_8'),
            mock.call._customize_and_sign_chrome(mock.ANY, mock.ANY,
                                                 '/$W_1/stable-AHHHH',
                                                 mock.ANY),
            mock.call.shutil.rmtree('/$W_9'),

            # Packaging and signing.
            mock.call._package_and_sign_dmg(
                self.paths.replace_work('/$W_1/stable'), mock.ANY),
            mock.call._package_and_sign_dmg(
                self.paths.replace_work('/$W_1/stable-MOO'), mock.ANY),
            mock.call._package_and_sign_pkg(
                self.paths.replace_work('/$W_1/stable'), mock.ANY),
            mock.call._package_zip(
                self.paths.replace_work('/$W_1/stable-MEOW'), mock.ANY),
            mock.call._package_and_sign_dmg(
                self.paths.replace_work('/$W_1/stable-MOOF'), mock.ANY),
            mock.call._package_and_sign_pkg(
                self.paths.replace_work('/$W_1/stable-MOOF'), mock.ANY),
            mock.call._package_and_sign_pkg(
                self.paths.replace_work('/$W_1/stable'), mock.ANY),
            mock.call._package_zip(
                self.paths.replace_work('/$W_1/stable'), mock.ANY),
            mock.call._package_and_sign_dmg(
                self.paths.replace_work('/$W_1/stable-MOOEOW'), mock.ANY),
            mock.call._package_zip(
                self.paths.replace_work('/$W_1/stable-MOOEOW'), mock.ANY),
            mock.call._package_and_sign_dmg(
                self.paths.replace_work('/$W_1/stable-AHHHH'), mock.ANY),
            mock.call._package_and_sign_pkg(
                self.paths.replace_work('/$W_1/stable-AHHHH'), mock.ANY),
            mock.call._package_zip(
                self.paths.replace_work('/$W_1/stable-AHHHH'), mock.ANY),
            mock.call.shutil.rmtree('/$W_1'),

            # Finally the installer tools.
            mock.call._package_installer_tools(mock.ANY, mock.ANY),
        ])

    @mock.patch('signing.pipeline._filter_distributions', _filter_distributions)
    def test_sign_calls_filters(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        skip_brands = ['MOO']
        include_channels = ['beta']

        class Config(test_config.TestConfig):

            @property
            def distributions(self):
                return [
                    model.Distribution(),
                ]

        config = Config(notarize=model.NotarizeAndStapleLevel.NONE)
        pipeline.sign_all(
            self.paths,
            config,
            skip_brands=skip_brands,
            channels=include_channels)

        self.assertEqual(_last_brand_filter(), skip_brands)
        self.assertEqual(_last_channel_filter(), include_channels)
