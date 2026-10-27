# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import asyncio
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
    if p == '/$I/Product Packaging/pkg_preinstall.in':
        return """app dir is '@APP_DIR@'
bundle_id is '@BUNDLE_ID@'"""

    if p == '/$I/Product Packaging/pkg_postinstall.in':
        return """app dir is '@APP_DIR@'
app product is '@APP_PRODUCT@'
brand code is '@BRAND_CODE@'
framework dir is '@FRAMEWORK_DIR@'
updater requirement is '@UPDATER_REQUIREMENT@'"""

    raise


def _get_adjacent_item(l, o):  # noqa: E741
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
    'signing.commands',
    **{
        m: mock.DEFAULT
        for m in (
            'move_file',
            'copy_files',
            'copy_dir_overwrite_and_count_changes',
            'run_command',
            'make_dir',
            'shutil',
            'write_file',
            'set_executable',
        )
    },
)
@mock.patch.multiple(
    'signing.signing', **{m: mock.DEFAULT for m in ('sign_part', 'verify_part')}
)
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

        asyncio.run(
            pipeline._customize_and_sign_chrome(paths, dist_config, '$D', None)
        )

        manager.assert_has_calls(
            [
                mock.call.copy_files('/$I/App Product.app', '/$W'),
                mock.call.customize_distribution(paths, dist, dist_config),
                mock.call.copy_dir_overwrite_and_count_changes(
                    '/$W/App Product.app/Contents/Frameworks/Product Framework.framework',
                    '/$W/modified_unsigned_framework',
                    dry_run=False,
                ),
                mock.call.sign_chrome(paths, dist_config, sign_framework=True),
                mock.call.copy_dir_overwrite_and_count_changes(
                    '/$W/App Product.app/Contents/Frameworks/Product Framework.framework',
                    '/$W/modified_unsigned_framework',
                    dry_run=True,
                ),
                mock.call.make_dir('$D'),
                mock.call.move_file(
                    '/$W/App Product.app', '$D/App Product.app'
                ),
            ]
        )

    @mock.patch('signing.modification.customize_distribution')
    def test_customize_and_sign_chrome_customize_different_product(
        self, customize, **kwargs
    ):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)
        manager.attach_mock(customize, 'customize_distribution')

        dist = model.Distribution(
            channel_customize=True,
            channel='canary',
            app_name_fragment='Canary',
            product_dirname='canary',
            creator_code='cana',
        )
        config = test_config.TestConfig()
        dist_config = dist.to_config(config)
        paths = self.paths.replace_work('/$W')

        asyncio.run(
            pipeline._customize_and_sign_chrome(paths, dist_config, '$D', None)
        )

        manager.assert_has_calls(
            [
                mock.call.copy_files('/$I/App Product.app', '/$W'),
                mock.call.customize_distribution(paths, dist, dist_config),
                mock.call.copy_dir_overwrite_and_count_changes(
                    '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework',
                    '/$W/modified_unsigned_framework',
                    dry_run=False,
                ),
                mock.call.sign_chrome(paths, dist_config, sign_framework=True),
                mock.call.copy_dir_overwrite_and_count_changes(
                    '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework',
                    '/$W/modified_unsigned_framework',
                    dry_run=True,
                ),
                mock.call.make_dir('$D'),
                mock.call.move_file(
                    '/$W/App Product Canary.app', '$D/App Product Canary.app'
                ),
            ]
        )

    @mock.patch('signing.modification.customize_distribution')
    def test_customize_and_sign_chrome_customize_multiple_times(
        self, customize, **kwargs
    ):
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
        asyncio.run(
            pipeline._customize_and_sign_chrome(
                paths, base_dist_config, notary_paths.work, signed_frameworks
            )
        )

        branded_dist = model.Distribution(
            branding_code='c0de', packaging_name_fragment='Branded'
        )
        branded_dist_config = branded_dist.to_config(config)
        paths = self.paths.replace_work('/$W')

        asyncio.run(
            pipeline._customize_and_sign_chrome(
                paths, branded_dist_config, notary_paths.work, signed_frameworks
            )
        )

        channel_dist = model.Distribution(
            channel_customize=True,
            channel='canary',
            app_name_fragment='Canary',
            product_dirname='canary',
            creator_code='cana',
        )
        channel_dist_config = channel_dist.to_config(config)
        paths = self.paths.replace_work('/$W')

        asyncio.run(
            pipeline._customize_and_sign_chrome(
                paths, channel_dist_config, notary_paths.work, signed_frameworks
            )
        )

        manager.assert_has_calls(
            [
                mock.call.copy_files('/$I/App Product.app', '/$W'),
                mock.call.customize_distribution(
                    paths, base_dist, base_dist_config
                ),
                mock.call.copy_dir_overwrite_and_count_changes(
                    '/$W/App Product.app/Contents/Frameworks/Product Framework.framework',
                    '/$W/modified_unsigned_framework',
                    dry_run=False,
                ),
                mock.call.sign_chrome(
                    paths, base_dist_config, sign_framework=True
                ),
                mock.call.copy_dir_overwrite_and_count_changes(
                    '/$W/App Product.app/Contents/Frameworks/Product Framework.framework',
                    '/$W/modified_unsigned_framework',
                    dry_run=True,
                ),
                mock.call.make_dir('/$D'),
                mock.call.move_file(
                    '/$W/App Product.app', '/$D/App Product.app'
                ),
                mock.call.copy_files('/$I/App Product.app', '/$W'),
                mock.call.customize_distribution(
                    paths, branded_dist, branded_dist_config
                ),
                mock.call.copy_dir_overwrite_and_count_changes(
                    '/$D/App Product.app/Contents/Frameworks/Product Framework.framework',
                    '/$W/App Product.app/Contents/Frameworks/Product Framework.framework',
                    dry_run=False,
                ),
                mock.call.sign_chrome(
                    paths, branded_dist_config, sign_framework=False
                ),
                mock.call.make_dir('/$D'),
                mock.call.move_file(
                    '/$W/App Product.app', '/$D/App Product.app'
                ),
                mock.call.copy_files('/$I/App Product.app', '/$W'),
                mock.call.customize_distribution(
                    paths, channel_dist, channel_dist_config
                ),
                mock.call.copy_dir_overwrite_and_count_changes(
                    '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework',
                    '/$W/modified_unsigned_framework',
                    dry_run=False,
                ),
                mock.call.sign_chrome(
                    paths, channel_dist_config, sign_framework=True
                ),
                mock.call.copy_dir_overwrite_and_count_changes(
                    '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework',
                    '/$W/modified_unsigned_framework',
                    dry_run=True,
                ),
                mock.call.make_dir('/$D'),
                mock.call.move_file(
                    '/$W/App Product Canary.app', '/$D/App Product Canary.app'
                ),
            ]
        )

    @mock.patch('signing.notarize.staple')
    def test_staple_chrome_no_customize(self, staple, **kwargs):
        dist = model.Distribution()
        paths = self.paths.replace_work('/$W')

        pipeline._staple_chrome(paths, dist.to_config(test_config.TestConfig()))

        self.assertEqual(
            staple.mock_calls,
            [
                mock.call(
                    '/$W/App Product.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper.app'
                ),
                mock.call(
                    '/$W/App Product.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Renderer).app'
                ),
                mock.call(
                    '/$W/App Product.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (GPU).app'
                ),
                mock.call(
                    '/$W/App Product.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Aperitif).app'
                ),
                mock.call(
                    '/$W/App Product.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Aperitif Renderer).app'
                ),
                mock.call(
                    '/$W/App Product.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Aperitif GPU).app'
                ),
                mock.call(
                    '/$W/App Product.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Aperitif Alerts).app'
                ),
                mock.call(
                    '/$W/App Product.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Alerts).app'
                ),
                mock.call('/$W/App Product.app'),
            ],
        )

    @mock.patch('signing.notarize.staple')
    def test_staple_chrome_customize(self, staple, **kwargs):
        dist = model.Distribution(
            channel_customize=True,
            channel='canary',
            app_name_fragment='Canary',
            product_dirname='canary',
            creator_code='cana',
        )
        paths = self.paths.replace_work('/$W')

        pipeline._staple_chrome(paths, dist.to_config(test_config.TestConfig()))

        self.assertEqual(
            staple.mock_calls,
            [
                mock.call(
                    '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper.app'
                ),
                mock.call(
                    '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Renderer).app'
                ),
                mock.call(
                    '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (GPU).app'
                ),
                mock.call(
                    '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Aperitif).app'
                ),
                mock.call(
                    '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Aperitif Renderer).app'
                ),
                mock.call(
                    '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Aperitif GPU).app'
                ),
                mock.call(
                    '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Aperitif Alerts).app'
                ),
                mock.call(
                    '/$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/Helpers/Product Helper (Alerts).app'
                ),
                mock.call('/$W/App Product Canary.app'),
            ],
        )

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
            package_as_zip=False,
        )
        dist_config = dist.to_config(test_config.TestConfig())
        paths = self.paths.replace_work('/$W')

        self.assertEqual(
            '/$W/scripts', pipeline._create_pkgbuild_scripts(paths, dist_config)
        )

        manager.assert_has_calls(
            [
                mock.call.make_dir('/$W/scripts'),
                mock.call.write_file('/$W/scripts/preinstall', mock.ANY),
                mock.call.set_executable('/$W/scripts/preinstall'),
                mock.call.write_file('/$W/scripts/postinstall', mock.ANY),
                mock.call.set_executable('/$W/scripts/postinstall'),
            ]
        )

        preinstall_string = manager.mock_calls[1][1][1]
        self.assertEqual(
            preinstall_string,
            """app dir is 'App Product.app'
bundle_id is 'test.signing.bundle_id'""",
        )

        postinstall_string = manager.mock_calls[3][1][1]
        self.assertEqual(
            postinstall_string,
            """app dir is 'App Product.app'
app product is 'App Product'
brand code is 'MOO'
framework dir is 'App Product.app/Contents/Frameworks/Product Framework.framework'
updater requirement is 'true '""",
        )

    @mock.patch('signing.pipeline.commands.write_plist', _write_plist)
    def test_component_property_path(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        dist = model.Distribution()
        dist_config = dist.to_config(test_config.TestConfig())
        paths = self.paths.replace_work('/$W')

        self.assertEqual(
            '/$W/App Product.plist',
            pipeline._component_property_path(paths, dist_config),
        )

        self.assertEqual(
            _last_written_plist(),
            [
                {
                    'BundleOverwriteAction': 'upgrade',
                    'BundleIsVersionChecked': True,
                    'BundleHasStrictIdentifier': True,
                    'RootRelativeBundlePath': 'App Product.app',
                    'BundleIsRelocatable': False,
                }
            ],
        )

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
            paths.work, '{}.pkg'.format(dist_config.app_product)
        )

        self.assertEqual(
            '/$W/App Product.dist',
            pipeline._productbuild_distribution_path(
                paths, paths, dist_config, component_pkg_path
            ),
        )

        manager.assert_has_calls(
            [
                mock.call.write_file('/$W/App Product.dist', mock.ANY),
            ]
        )

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
            package_as_dmg=True, package_as_pkg=False, package_as_zip=False
        )
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual(
            '/$O/AppProduct-99.0.9999.99.dmg',
            pipeline._package_and_sign_dmg(paths, dist_config),
        )

        manager.assert_has_calls(
            [
                mock.call.make_dir('/$W/empty'),
                mock.call.run_command(mock.ANY),
                mock.call.sign_part(paths, dist_config, mock.ANY),
                mock.call.verify_part(paths, mock.ANY),
            ]
        )

        run_command = [
            call for call in manager.mock_calls if call[0] == 'run_command'
        ][0]
        pkg_dmg_args = run_command[1][0]

        self.assertEqual(
            '/$O/AppProduct-99.0.9999.99.dmg',
            _get_adjacent_item(pkg_dmg_args, '--target'),
        )
        self.assertEqual(
            config.app_product, _get_adjacent_item(pkg_dmg_args, '--volname')
        )
        self.assertEqual(
            'AppProduct-99.0.9999.99',
            kwargs['sign_part'].mock_calls[0][1][2].identifier,
        )

    def test_package_zip_chrome_branded(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig()
        dist = model.Distribution(
            package_as_dmg=False, package_as_pkg=False, package_as_zip=True
        )
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual(
            '/$O/AppProduct-99.0.9999.99.zip',
            pipeline._package_zip(paths, dist_config),
        )

        manager.assert_has_calls(
            [
                mock.call.copy_files(
                    '/$I/Product Packaging/keystone_install.sh',
                    '/$W/.keystone_install',
                ),
                mock.call.run_command(
                    [
                        'zip',
                        '-9',
                        '--recurse-paths',
                        '--symlinks',
                        '--quiet',
                        '/$O/AppProduct-99.0.9999.99.zip',
                        'App Product.app',
                        '.keystone_install',
                    ],
                    cwd='/$W',
                ),
            ]
        )

    def test_package_zip_non_chrome_branded(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfigNonChromeBranded()
        dist = model.Distribution(
            package_as_dmg=False, package_as_pkg=False, package_as_zip=True
        )
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual(
            '/$O/AppProduct-99.0.9999.99.zip',
            pipeline._package_zip(paths, dist_config),
        )

        manager.assert_has_calls(
            [
                mock.call.run_command(
                    [
                        'zip',
                        '-9',
                        '--recurse-paths',
                        '--symlinks',
                        '--quiet',
                        '/$O/AppProduct-99.0.9999.99.zip',
                        'App Product.app',
                    ],
                    cwd='/$W',
                ),
            ]
        )

    def test_package_zip_chrome_branded_with_fragment(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig()
        dist = model.Distribution(
            packaging_name_fragment='imagination',
            package_as_dmg=False,
            package_as_pkg=False,
            package_as_zip=True,
        )
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual(
            '/$O/AppProduct-99.0.9999.99-imagination.zip',
            pipeline._package_zip(paths, dist_config),
        )

        manager.assert_has_calls(
            [
                mock.call.copy_files(
                    '/$I/Product Packaging/keystone_install.sh',
                    '/$W/.keystone_install',
                ),
                mock.call.run_command(
                    [
                        'zip',
                        '-9',
                        '--recurse-paths',
                        '--symlinks',
                        '--quiet',
                        '/$O/AppProduct-99.0.9999.99-imagination.zip',
                        'App Product.app',
                        '.keystone_install',
                    ],
                    cwd='/$W',
                ),
            ]
        )

    def test_package_zip_non_chrome_branded_with_fragment(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfigNonChromeBranded()
        dist = model.Distribution(
            packaging_name_fragment='imagination',
            package_as_dmg=False,
            package_as_pkg=False,
            package_as_zip=True,
        )
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual(
            '/$O/AppProduct-99.0.9999.99-imagination.zip',
            pipeline._package_zip(paths, dist_config),
        )

        manager.assert_has_calls(
            [
                mock.call.run_command(
                    [
                        'zip',
                        '-9',
                        '--recurse-paths',
                        '--symlinks',
                        '--quiet',
                        '/$O/AppProduct-99.0.9999.99-imagination.zip',
                        'App Product.app',
                    ],
                    cwd='/$W',
                )
            ]
        )

    @mock.patch(
        'signing.pipeline._component_property_path', _component_property_path
    )
    @mock.patch(
        'signing.pipeline._productbuild_distribution_path',
        _productbuild_distribution_path,
    )
    @mock.patch(
        'signing.pipeline._create_pkgbuild_scripts', _create_pkgbuild_scripts
    )
    @mock.patch('signing.pipeline._minimum_os_version', _minimum_os_version)
    def test_package_and_sign_pkg_no_branding(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig()
        dist = model.Distribution(
            package_as_dmg=False, package_as_pkg=True, package_as_zip=False
        )
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual(
            '/$O/AppProduct-99.0.9999.99.pkg',
            pipeline._package_and_sign_pkg(paths, dist_config),
        )

        manager.assert_has_calls(
            [
                mock.call.run_command(mock.ANY),
                mock.call.run_command(mock.ANY),
                mock.call.run_command(mock.ANY),
            ]
        )

        run_commands = [
            call for call in manager.mock_calls if call[0] == 'run_command'
        ]
        self.assertEqual(3, len(run_commands))
        pkgbuild_args = run_commands[0][1][0]
        productbuild_args = run_commands[1][1][0]
        pkgutil_args = run_commands[2][1][0]
        self.assertEqual(
            ['pkgutil', '--check-signature', '/$O/AppProduct-99.0.9999.99.pkg'],
            pkgutil_args,
        )

        self.assertEqual(
            '/$W_1/payload', _get_adjacent_item(pkgbuild_args, '--root')
        )
        self.assertEqual(
            '/$W_1/App Product.plist',
            _get_adjacent_item(pkgbuild_args, '--component-plist'),
        )
        self.assertEqual(
            'test.signing.bundle_id',
            _get_adjacent_item(pkgbuild_args, '--identifier'),
        )
        self.assertEqual(
            '99.0.9999.99', _get_adjacent_item(pkgbuild_args, '--version')
        )
        self.assertEqual(
            '/$W_1/scripts', _get_adjacent_item(pkgbuild_args, '--scripts')
        )

        self.assertEqual(
            'latest', _get_adjacent_item(pkgbuild_args, '--compression')
        )
        self.assertEqual(
            '10.11.0', _get_adjacent_item(pkgbuild_args, '--min-os-version')
        )

        self.assertEqual(
            'test.signing.bundle_id',
            _get_adjacent_item(productbuild_args, '--identifier'),
        )
        self.assertEqual(
            '99.0.9999.99', _get_adjacent_item(productbuild_args, '--version')
        )
        self.assertEqual(
            '/$W_1/App Product.dist',
            _get_adjacent_item(productbuild_args, '--distribution'),
        )
        self.assertEqual(
            '/$W_1', _get_adjacent_item(productbuild_args, '--package-path')
        )
        self.assertEqual(
            '[INSTALLER-IDENTITY]',
            _get_adjacent_item(productbuild_args, '--sign'),
        )

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
            package_as_zip=False,
        )
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual(
            '/$O/AppProduct-99.0.9999.99-ForCows.dmg',
            pipeline._package_and_sign_dmg(paths, dist_config),
        )

        manager.assert_has_calls(
            [
                mock.call.make_dir('/$W/empty'),
                mock.call.run_command(mock.ANY),
                mock.call.sign_part(paths, dist_config, mock.ANY),
                mock.call.verify_part(paths, mock.ANY),
            ]
        )

        run_command = [
            call for call in manager.mock_calls if call[0] == 'run_command'
        ][0]
        pkg_dmg_args = run_command[1][0]

        self.assertEqual(
            '/$O/AppProduct-99.0.9999.99-ForCows.dmg',
            _get_adjacent_item(pkg_dmg_args, '--target'),
        )
        self.assertEqual(
            config.app_product, _get_adjacent_item(pkg_dmg_args, '--volname')
        )
        self.assertEqual(
            'AppProduct-99.0.9999.99-MOO',
            kwargs['sign_part'].mock_calls[0][1][2].identifier,
        )

    @mock.patch(
        'signing.pipeline._component_property_path', _component_property_path
    )
    @mock.patch(
        'signing.pipeline._productbuild_distribution_path',
        _productbuild_distribution_path,
    )
    @mock.patch(
        'signing.pipeline._create_pkgbuild_scripts', _create_pkgbuild_scripts
    )
    @mock.patch('signing.pipeline._minimum_os_version', _minimum_os_version)
    def test_package_and_sign_pkg_branding(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig()
        dist = model.Distribution(
            branding_code='MOO',
            packaging_name_fragment='ForCows',
            package_as_dmg=False,
            package_as_pkg=True,
            package_as_zip=False,
        )
        dist_config = dist.to_config(config)

        paths = self.paths.replace_work('/$W')
        self.assertEqual(
            '/$O/AppProduct-99.0.9999.99-ForCows.pkg',
            pipeline._package_and_sign_pkg(paths, dist_config),
        )

        manager.assert_has_calls(
            [
                mock.call.run_command(mock.ANY),
                mock.call.run_command(mock.ANY),
                mock.call.run_command(mock.ANY),
            ]
        )

        run_commands = [
            call for call in manager.mock_calls if call[0] == 'run_command'
        ]
        self.assertEqual(3, len(run_commands))
        pkgbuild_args = run_commands[0][1][0]
        productbuild_args = run_commands[1][1][0]
        pkgutil_args = run_commands[2][1][0]
        self.assertEqual(
            [
                'pkgutil',
                '--check-signature',
                '/$O/AppProduct-99.0.9999.99-ForCows.pkg',
            ],
            pkgutil_args,
        )

        self.assertEqual(
            '/$W_1/payload', _get_adjacent_item(pkgbuild_args, '--root')
        )
        self.assertEqual(
            '/$W_1/App Product.plist',
            _get_adjacent_item(pkgbuild_args, '--component-plist'),
        )
        self.assertEqual(
            'test.signing.bundle_id',
            _get_adjacent_item(pkgbuild_args, '--identifier'),
        )
        self.assertEqual(
            '99.0.9999.99', _get_adjacent_item(pkgbuild_args, '--version')
        )
        self.assertEqual(
            '/$W_1/scripts', _get_adjacent_item(pkgbuild_args, '--scripts')
        )

        self.assertEqual(
            'latest', _get_adjacent_item(pkgbuild_args, '--compression')
        )
        self.assertEqual(
            '10.11.0', _get_adjacent_item(pkgbuild_args, '--min-os-version')
        )

        self.assertEqual(
            'test.signing.bundle_id',
            _get_adjacent_item(productbuild_args, '--identifier'),
        )
        self.assertEqual(
            '99.0.9999.99', _get_adjacent_item(productbuild_args, '--version')
        )
        self.assertEqual(
            '/$W_1/App Product.dist',
            _get_adjacent_item(productbuild_args, '--distribution'),
        )
        self.assertEqual(
            '/$W_1', _get_adjacent_item(productbuild_args, '--package-path')
        )
        self.assertEqual(
            '[INSTALLER-IDENTITY]',
            _get_adjacent_item(productbuild_args, '--sign'),
        )

    def test_verify_pkg_signature(self, **kwargs):
        pipeline._verify_pkg_signature('/$O/AppProduct-99.0.9999.99.pkg')
        kwargs['run_command'].assert_called_once_with(
            ['pkgutil', '--check-signature', '/$O/AppProduct-99.0.9999.99.pkg']
        )

    def test_taggable_pkg_path(self, **kwargs):
        config = test_config.TestConfig()

        def taggable_path(**dist_kwargs):
            dist = model.Distribution(package_as_pkg=True, **dist_kwargs)
            return pipeline._taggable_pkg_path(
                self.paths, dist.to_config(config)
            )

        # Fragment and brand: the fragment is used.
        self.assertEqual(
            '/$O/AppProduct-99.0.9999.99-Enterprise-Brand-GCEM-Taggable.pkg',
            taggable_path(
                branding_code='GCEM',
                packaging_name_fragment='Enterprise-Brand-GCEM',
            ),
        )
        # Brand without fragment: the brand code is used.
        self.assertEqual(
            '/$O/AppProduct-99.0.9999.99-GCEM-Taggable.pkg',
            taggable_path(branding_code='GCEM'),
        )
        # Neither: 'Unbranded' is used.
        self.assertEqual(
            '/$O/AppProduct-99.0.9999.99-Unbranded-Taggable.pkg',
            taggable_path(),
        )

    def test_derived_pkg_paths_use_packaging_basename(self, **kwargs):
        # Derived PKGs are always prefixed by the distribution's
        # packaging_basename, including when a config customizes it.
        class Config(test_config.TestConfig):
            @property
            def packaging_basename(self):
                return 'Custom-Basename'

        config = Config()
        for dist in (
            model.Distribution(
                branding_code='GCEM',
                packaging_name_fragment='Enterprise',
                package_as_pkg=True,
            ),
            model.Distribution(branding_code='GCEM', package_as_pkg=True),
        ):
            dist_config = dist.to_config(config)
            prefix = '/$O/' + dist_config.packaging_basename
            self.assertEqual(
                prefix + '.pkg', pipeline._pkg_path(self.paths, dist_config)
            )
            self.assertTrue(
                pipeline._taggable_pkg_path(self.paths, dist_config).startswith(
                    prefix + '-'
                )
            )
            self.assertTrue(
                pipeline._variant_pkg_path(
                    self.paths, dist_config, 'GCSW'
                ).startswith(prefix + '-')
            )

        # Without a packaging name fragment, the customized basename is used.
        dist_config = model.Distribution(
            branding_code='GCEM', package_as_pkg=True
        ).to_config(config)
        self.assertEqual(
            '/$O/Custom-Basename-GCEM-Tagged-GCSW.pkg',
            pipeline._variant_pkg_path(self.paths, dist_config, 'GCSW'),
        )

    def test_variant_pkg_path(self, **kwargs):
        config = test_config.TestConfig()

        def variant_path(variant_brand, **dist_kwargs):
            dist = model.Distribution(package_as_pkg=True, **dist_kwargs)
            return pipeline._variant_pkg_path(
                self.paths, dist.to_config(config), variant_brand
            )

        # Fragment and brand: the fragment is used verbatim, even when it
        # contains the brand code.
        self.assertEqual(
            (
                '/$O/AppProduct-99.0.9999.99-'
                'Stable-Enterprise-Brand-GCEM-Tagged-GCSW.pkg'
            ),
            variant_path(
                'GCSW',
                branding_code='GCEM',
                packaging_name_fragment='Stable-Enterprise-Brand-GCEM',
            ),
        )
        # Fragment without brand.
        self.assertEqual(
            '/$O/AppProduct-99.0.9999.99-Stable-Enterprise-Tagged-GCSW.pkg',
            variant_path('GCSW', packaging_name_fragment='Stable-Enterprise'),
        )
        # Brand without fragment: the brand code is used.
        self.assertEqual(
            '/$O/AppProduct-99.0.9999.99-GCEM-Tagged-GCSW.pkg',
            variant_path('GCSW', branding_code='GCEM'),
        )
        # Neither: 'Unbranded' is used.
        self.assertEqual(
            '/$O/AppProduct-99.0.9999.99-Unbranded-Tagged-GCSW.pkg',
            variant_path('GCSW'),
        )
        # Re-tagging with the distribution's own brand needs no special case.
        self.assertEqual(
            '/$O/AppProduct-99.0.9999.99-Enterprise-Brand-GCEM-Tagged-GCEM.pkg',
            variant_path(
                'GCEM',
                branding_code='GCEM',
                packaging_name_fragment='Enterprise-Brand-GCEM',
            ),
        )
        # Different source distributions tagged with the same brand produce
        # different names.
        self.assertNotEqual(
            variant_path('GCSW', branding_code='GCEM'),
            variant_path('GCSW', branding_code='GCOA'),
        )

    def test_tag_pkg_distributions_no_tag_tool(self, **kwargs):
        dist = model.Distribution(package_as_pkg=True, tagged_variants=[])
        dist_config = dist.to_config(test_config.TestConfig())
        pkg_dists = [
            pipeline.PkgDistribution(
                dist, dist_config, self.paths, '/$O/base.pkg'
            )
        ]
        with self.assertRaises(ValueError) as cm:
            pipeline._tag_pkg_distributions(pkg_dists, tag_tool=None)
        self.assertEqual(
            'tag_tool must be specified when a distribution '
            'has tagged_variants',
            str(cm.exception),
        )

    @mock.patch('signing.logger.warning')
    def test_tag_pkg_distributions_none_variants_no_tag_tool(
        self, warning, **kwargs
    ):
        dist = model.Distribution(package_as_pkg=True, tagged_variants=None)
        dist_config = dist.to_config(test_config.TestConfig())
        pkg_dists = [
            pipeline.PkgDistribution(
                dist, dist_config, self.paths, '/$O/base.pkg'
            )
        ]
        pipeline._tag_pkg_distributions(pkg_dists, tag_tool=None)
        kwargs['run_command'].assert_not_called()
        warning.assert_not_called()

    def test_tag_pkg_distributions_none_variants(self, **kwargs):
        dist = model.Distribution(package_as_pkg=True, tagged_variants=None)
        dist_config = dist.to_config(test_config.TestConfig())
        pkg_dists = [
            pipeline.PkgDistribution(
                dist, dist_config, self.paths, '/$O/base.pkg'
            )
        ]
        pipeline._tag_pkg_distributions(pkg_dists, tag_tool='/path/to/tag')
        kwargs['run_command'].assert_not_called()

    def test_tag_pkg_distributions_empty_variants(self, **kwargs):
        dist = model.Distribution(package_as_pkg=True, tagged_variants=[])
        dist_config = dist.to_config(test_config.TestConfig())
        pkg_dists = [
            pipeline.PkgDistribution(
                dist, dist_config, self.paths, '/$O/base.pkg'
            )
        ]
        taggable_path = '/$O/AppProduct-99.0.9999.99-Unbranded-Taggable.pkg'
        pipeline._tag_pkg_distributions(pkg_dists, tag_tool='/path/to/tag')
        self.assertEqual(
            [
                mock.call(
                    [
                        '/path/to/tag',
                        '--set-tag',
                        '--out=' + taggable_path,
                        '/$O/base.pkg',
                    ]
                ),
                mock.call(
                    [
                        'pkgutil',
                        '--check-signature',
                        taggable_path,
                    ]
                ),
                mock.call(
                    [
                        'xcrun',
                        'stapler',
                        'validate',
                        taggable_path,
                    ]
                ),
            ],
            kwargs['run_command'].mock_calls,
        )

    def test_tag_pkg_distributions_with_variants(self, **kwargs):
        dist = model.Distribution(
            branding_code='GCEM',
            packaging_name_fragment='Enterprise-Brand-GCEM',
            package_as_pkg=True,
            tagged_variants=['GCSW', 'GCOA'],
        )
        dist_config = dist.to_config(test_config.TestConfig())
        pkg_dists = [
            pipeline.PkgDistribution(
                dist, dist_config, self.paths, '/$O/Enterprise-Brand-GCEM.pkg'
            )
        ]
        taggable_path = (
            '/$O/AppProduct-99.0.9999.99-Enterprise-Brand-GCEM-Taggable.pkg'
        )
        gcsw_path = (
            '/$O/AppProduct-99.0.9999.99-Enterprise-Brand-GCEM-Tagged-GCSW.pkg'
        )
        gcoa_path = (
            '/$O/AppProduct-99.0.9999.99-Enterprise-Brand-GCEM-Tagged-GCOA.pkg'
        )
        pipeline._tag_pkg_distributions(pkg_dists, tag_tool='/path/to/tag')
        self.assertEqual(
            [
                mock.call(
                    [
                        '/path/to/tag',
                        '--set-tag',
                        '--out=' + taggable_path,
                        '/$O/Enterprise-Brand-GCEM.pkg',
                    ]
                ),
                mock.call(['pkgutil', '--check-signature', taggable_path]),
                mock.call(['xcrun', 'stapler', 'validate', taggable_path]),
                mock.call(
                    [
                        '/path/to/tag',
                        '--set-tag=brand=GCSW',
                        '--out=' + gcsw_path,
                        taggable_path,
                    ]
                ),
                mock.call(
                    [
                        'pkgutil',
                        '--check-signature',
                        gcsw_path,
                    ]
                ),
                mock.call(
                    [
                        'xcrun',
                        'stapler',
                        'validate',
                        gcsw_path,
                    ]
                ),
                mock.call(
                    [
                        '/path/to/tag',
                        '--set-tag=brand=GCOA',
                        '--out=' + gcoa_path,
                        taggable_path,
                    ]
                ),
                mock.call(
                    [
                        'pkgutil',
                        '--check-signature',
                        gcoa_path,
                    ]
                ),
                mock.call(
                    [
                        'xcrun',
                        'stapler',
                        'validate',
                        gcoa_path,
                    ]
                ),
            ],
            kwargs['run_command'].mock_calls,
        )

    def test_tag_pkg_distributions_base_pkg_never_modified_in_place(
        self, **kwargs
    ):
        base_pkg = '/$O/base.pkg'
        dist = model.Distribution(
            package_as_pkg=True,
            tagged_variants=['GCSW'],
        )
        dist_config = dist.to_config(
            test_config.TestConfig(notarize=model.NotarizeAndStapleLevel.NONE)
        )
        pkg_dists = [
            pipeline.PkgDistribution(dist, dist_config, self.paths, base_pkg)
        ]
        pipeline._tag_pkg_distributions(pkg_dists, tag_tool='/path/to/tag')
        tag_tool_calls = [
            call[0][0]
            for call in kwargs['run_command'].call_args_list
            if call[0][0][0] == '/path/to/tag'
        ]
        self.assertTrue(tag_tool_calls)
        for cmd in tag_tool_calls:
            self.assertTrue(any(arg.startswith('--out=') for arg in cmd))
            self.assertNotEqual(['/path/to/tag', '--set-tag', base_pkg], cmd)

    def test_tag_pkg_distributions_own_brand_variant(self, **kwargs):
        dist = model.Distribution(
            branding_code='GCEM',
            package_as_pkg=True,
            tagged_variants=['GCEM'],
        )
        dist_config = dist.to_config(test_config.TestConfig())
        pkg_path = '/$O/base.pkg'
        pkg_dists = [
            pipeline.PkgDistribution(dist, dist_config, self.paths, pkg_path)
        ]
        taggable_path = '/$O/AppProduct-99.0.9999.99-GCEM-Taggable.pkg'
        variant_path = '/$O/AppProduct-99.0.9999.99-GCEM-Tagged-GCEM.pkg'
        pipeline._tag_pkg_distributions(pkg_dists, tag_tool='/path/to/tag')
        kwargs['run_command'].assert_has_calls(
            [
                mock.call(
                    [
                        '/path/to/tag',
                        '--set-tag',
                        '--out=' + taggable_path,
                        '/$O/base.pkg',
                    ]
                ),
                mock.call(['pkgutil', '--check-signature', taggable_path]),
                mock.call(['xcrun', 'stapler', 'validate', taggable_path]),
                mock.call(
                    [
                        '/path/to/tag',
                        '--set-tag=brand=GCEM',
                        '--out=' + variant_path,
                        taggable_path,
                    ]
                ),
                mock.call(['pkgutil', '--check-signature', variant_path]),
                mock.call(['xcrun', 'stapler', 'validate', variant_path]),
            ]
        )

    def test_verify_pkg_paths_path_collision(self, **kwargs):
        # A hand-written fragment that matches a variant name still collides.
        dist1 = model.Distribution(
            packaging_name_fragment='Unbranded-Tagged-GCSW',
            package_as_pkg=True,
        )
        dist2 = model.Distribution(
            package_as_pkg=True,
            tagged_variants=['GCSW'],
        )
        config = test_config.TestConfig()
        with self.assertRaises(ValueError) as cm:
            pipeline._verify_pkg_paths_noncolliding(
                [dist1, dist2], config, self.paths
            )
        self.assertEqual(
            "Duplicate package output path for tagged variant 'GCSW': "
            '/$O/AppProduct-99.0.9999.99-Unbranded-Tagged-GCSW.pkg',
            str(cm.exception),
        )

    def test_verify_pkg_paths_variant_and_native_brand_do_not_collide(
        self, **kwargs
    ):
        # A natively branded PKG and a variant tagged with the same brand get
        # different names.
        dist1 = model.Distribution(
            branding_code='GCSW',
            packaging_name_fragment='Brand-GCSW',
            package_as_pkg=True,
        )
        dist2 = model.Distribution(
            package_as_pkg=True,
            tagged_variants=['GCSW'],
        )
        config = test_config.TestConfig()
        pipeline._verify_pkg_paths_noncolliding(
            [dist1, dist2], config, self.paths
        )

    def test_verify_pkg_paths_duplicate_variant_collision(self, **kwargs):
        # Both distributions have the tagging base fragment 'GCEM' (one from
        # its brand code, one from its fragment) but distinct base PKGs.
        dist1 = model.Distribution(
            branding_code='GCEM',
            package_as_pkg=True,
            tagged_variants=['GCSW'],
        )
        dist2 = model.Distribution(
            packaging_name_fragment='GCEM',
            package_as_pkg=True,
            tagged_variants=['GCSW'],
        )
        config = test_config.TestConfig()
        with self.assertRaises(ValueError) as cm:
            pipeline._verify_pkg_paths_noncolliding(
                [dist1, dist2], config, self.paths
            )
        self.assertEqual(
            'Duplicate package output path for taggable package: '
            '/$O/AppProduct-99.0.9999.99-GCEM-Taggable.pkg\n'
            "Duplicate package output path for tagged variant 'GCSW': "
            '/$O/AppProduct-99.0.9999.99-GCEM-Tagged-GCSW.pkg',
            str(cm.exception),
        )

    def test_verify_pkg_paths_same_variant_different_sources(self, **kwargs):
        # The same variant brand built from different distributions does not
        # collide.
        dist1 = model.Distribution(
            branding_code='GCEM',
            packaging_name_fragment='Enterprise-Brand-GCEM',
            package_as_pkg=True,
            tagged_variants=['GCSW'],
        )
        dist2 = model.Distribution(
            branding_code='GCOA',
            packaging_name_fragment='Enterprise-Brand-GCOA',
            package_as_pkg=True,
            tagged_variants=['GCSW'],
        )
        config = test_config.TestConfig()
        pipeline._verify_pkg_paths_noncolliding(
            [dist1, dist2], config, self.paths
        )

    def test_verify_pkg_paths_taggable_package_collision(self, **kwargs):
        dist1 = model.Distribution(
            packaging_name_fragment='Unbranded-Taggable',
            package_as_pkg=True,
        )
        dist2 = model.Distribution(
            package_as_pkg=True,
            tagged_variants=[],
        )
        config = test_config.TestConfig()
        with self.assertRaises(ValueError) as cm:
            pipeline._verify_pkg_paths_noncolliding(
                [dist1, dist2], config, self.paths
            )
        self.assertIn(
            'Duplicate package output path for taggable package: '
            '/$O/AppProduct-99.0.9999.99-Unbranded-Taggable.pkg',
            str(cm.exception),
        )

    def test_verify_pkg_paths_duplicate_base_package_collision(self, **kwargs):
        dist1 = model.Distribution(package_as_pkg=True, tagged_variants=[])
        dist2 = model.Distribution(package_as_pkg=True, tagged_variants=[])
        config = test_config.TestConfig()
        with self.assertRaises(ValueError) as cm:
            pipeline._verify_pkg_paths_noncolliding(
                [dist1, dist2], config, self.paths
            )
        self.assertIn(
            'Duplicate package output path: /$O/AppProduct-99.0.9999.99.pkg',
            str(cm.exception),
        )

    def test_verify_pkg_paths_reports_all_collisions(self, **kwargs):
        dist1 = model.Distribution(package_as_pkg=True, tagged_variants=[])
        dist2 = model.Distribution(package_as_pkg=True, tagged_variants=[])
        config = test_config.TestConfig()
        with self.assertRaises(ValueError) as cm:
            pipeline._verify_pkg_paths_noncolliding(
                [dist1, dist2], config, self.paths
            )
        self.assertEqual(
            'Duplicate package output path: '
            '/$O/AppProduct-99.0.9999.99.pkg\n'
            'Duplicate package output path for taggable package: '
            '/$O/AppProduct-99.0.9999.99-Unbranded-Taggable.pkg',
            str(cm.exception),
        )

    def test_package_dmg_no_customize(self, **kwargs):
        dist = model.Distribution()
        config = test_config.TestConfig()
        paths = self.paths.replace_work('/$W')

        dmg_path = pipeline._package_dmg(paths, dist, config)
        self.assertEqual('/$O/AppProduct-99.0.9999.99.dmg', dmg_path)

        pkg_dmg_args = kwargs['run_command'].mock_calls[0][1][0]

        self.assertEqual(dmg_path, _get_adjacent_item(pkg_dmg_args, '--target'))
        self.assertEqual(
            '/$I/Product Packaging/chrome_dmg_icon.icns',
            _get_adjacent_item(pkg_dmg_args, '--icon'),
        )
        self.assertEqual(
            'App Product', _get_adjacent_item(pkg_dmg_args, '--volname')
        )
        self.assertEqual(
            '/$W/empty', _get_adjacent_item(pkg_dmg_args, '--source')
        )

        copy_specs = [
            pkg_dmg_args[i + 1]
            for i, arg in enumerate(pkg_dmg_args)
            if arg == '--copy'
        ]
        self.assertEqual(
            set(copy_specs),
            set(
                [
                    '/$W/App Product.app:/',
                    '/$I/Product Packaging/keystone_install.sh:/.keystone_install',
                    '/$I/Product Packaging/chrome_dmg_background.png:/.background/background.png',
                    '/$I/Product Packaging/chrome_dmg_dsstore:/.DS_Store',
                ]
            ),
        )

    def test_package_dmg_customize(self, **kwargs):
        dist = model.Distribution(
            channel_customize=True,
            channel='canary',
            app_name_fragment='Canary',
            product_dirname='canary',
            creator_code='cana',
        )
        config = dist.to_config(test_config.TestConfig())
        paths = self.paths.replace_work('/$W')

        dmg_path = pipeline._package_dmg(paths, dist, config)
        self.assertEqual('/$O/AppProductCanary-99.0.9999.99.dmg', dmg_path)

        pkg_dmg_args = kwargs['run_command'].mock_calls[0][1][0]

        self.assertEqual(dmg_path, _get_adjacent_item(pkg_dmg_args, '--target'))
        self.assertEqual(
            '/$I/Product Packaging/chrome_canary_dmg_icon.icns',
            _get_adjacent_item(pkg_dmg_args, '--icon'),
        )
        self.assertEqual(
            'App Product Canary', _get_adjacent_item(pkg_dmg_args, '--volname')
        )
        self.assertEqual(
            '/$W/empty', _get_adjacent_item(pkg_dmg_args, '--source')
        )

        copy_specs = [
            pkg_dmg_args[i + 1]
            for i, arg in enumerate(pkg_dmg_args)
            if arg == '--copy'
        ]
        self.assertEqual(
            set(copy_specs),
            set(
                [
                    '/$W/App Product Canary.app:/',
                    '/$I/Product Packaging/keystone_install.sh:/.keystone_install',
                    '/$I/Product Packaging/chrome_dmg_background.png:/.background/background.png',
                    '/$I/Product Packaging/chrome_canary_dmg_dsstore:/.DS_Store',
                ]
            ),
        )

    def test_package_dmg_no_customize_not_chrome(self, **kwargs):
        dist = model.Distribution()
        config = test_config.TestConfigNonChromeBranded()
        paths = self.paths.replace_work('/$W')

        dmg_path = pipeline._package_dmg(paths, dist, config)
        self.assertEqual('/$O/AppProduct-99.0.9999.99.dmg', dmg_path)

        pkg_dmg_args = kwargs['run_command'].mock_calls[0][1][0]

        self.assertEqual(dmg_path, _get_adjacent_item(pkg_dmg_args, '--target'))
        self.assertEqual(
            'App Product', _get_adjacent_item(pkg_dmg_args, '--volname')
        )
        self.assertEqual(
            '/$W/empty', _get_adjacent_item(pkg_dmg_args, '--source')
        )

        copy_specs = [
            pkg_dmg_args[i + 1]
            for i, arg in enumerate(pkg_dmg_args)
            if arg == '--copy'
        ]
        self.assertEqual(set(copy_specs), set(['/$W/App Product.app:/']))

    def test_filter_distributions(self, **kwargs):
        dist1 = model.Distribution()
        dist2 = model.Distribution(branding_code='MOO', channel='beta')
        dist3 = model.Distribution(branding_code='ARF', channel='dev')
        dist4 = model.Distribution(branding_code='MOOF', channel='canary')

        distributions = [dist1, dist2, dist3, dist4]

        # --- Neither ---

        # No filters should yield no change to the distribution list.
        self.assertEqual(
            distributions, pipeline._filter_distributions(distributions, [], [])
        )

        # --- Brands only ---

        # Filtering a brand code not being built should throw.
        with self.assertRaises(ValueError) as cm:
            pipeline._filter_distributions(distributions, ['MOOG'], [])
        self.assertEqual(
            cm.exception.args[0],
            "Brand codes do not match any distribution: ['MOOG']",
        )

        # Filtering one or more brand codes explicitly should remove them.
        self.assertEqual(
            [dist1, dist2, dist3],
            pipeline._filter_distributions(distributions, ['MOOF'], []),
        )
        self.assertEqual(
            [dist1, dist3],
            pipeline._filter_distributions(distributions, ['MOO', 'MOOF'], []),
        )

        # Filtering a '*' should remove all brand coded distributions.
        self.assertEqual(
            [dist1], pipeline._filter_distributions(distributions, ['*'], [])
        )

        # Filtering a specific brand code and '*' should remove all brand coded
        # distributions.
        self.assertEqual(
            [dist1],
            pipeline._filter_distributions(distributions, ['*', 'MOOF'], []),
        )

        # Filtering all brand codes when there aren't any should yield no change
        # to the distribution list.
        self.assertEqual(
            [dist1], pipeline._filter_distributions([dist1], ['*'], [])
        )

        # --- Channels ---

        # Filtering for a channel not being built should throw.
        with self.assertRaises(ValueError) as cm:
            pipeline._filter_distributions(distributions, [], ['hyper'])
        self.assertEqual(
            cm.exception.args[0],
            "Channels do not match any distribution: %r" % {'hyper'},
        )

        # Filtering for 'stable' should result in the distribution with None
        # as a channel.
        self.assertEqual(
            [dist1],
            pipeline._filter_distributions(distributions, [], ['stable']),
        )

        # Filtering for any other string as channel name should work.
        self.assertEqual(
            [dist2], pipeline._filter_distributions(distributions, [], ['beta'])
        )

        # Filtering for 'stable' along with other strings should work.
        self.assertEqual(
            [dist1, dist3],
            pipeline._filter_distributions(
                distributions, [], ['stable', 'dev']
            ),
        )

        # --- Both ---

        # Filtering on both in a way that allows a result should work.
        self.assertEqual(
            [dist2],
            pipeline._filter_distributions(distributions, ['MOOF'], ['beta']),
        )

        # Filtering for inclusion of a channel that is filtered out due to brand
        # code should throw.
        with self.assertRaises(ValueError) as cm:
            pipeline._filter_distributions(distributions, ['MOO'], ['beta'])
        self.assertEqual(
            cm.exception.args[0],
            "All distributions for channels were filtered out by brand: %r"
            % {'beta'},
        )

        # --- Tagged Variants ---

        dist_variants = model.Distribution(
            branding_code='BASE',
            package_as_pkg=True,
            tagged_variants=['VAR1', 'VAR2', 'VAR3'],
        )

        # Filtering one variant
        filtered = pipeline._filter_distributions([dist_variants], ['VAR2'], [])
        self.assertEqual(1, len(filtered))
        self.assertEqual('BASE', filtered[0].branding_code)
        self.assertEqual(('VAR1', 'VAR3'), filtered[0].tagged_variants)

        # Filtering multiple variants
        filtered = pipeline._filter_distributions(
            [dist_variants], ['VAR1', 'VAR3'], []
        )
        self.assertEqual(1, len(filtered))
        self.assertEqual(('VAR2',), filtered[0].tagged_variants)

        # Filtering '*' for a branded distribution removes the entire
        # distribution.
        filtered = pipeline._filter_distributions([dist_variants], ['*'], [])
        self.assertEqual(0, len(filtered))

        # Filtering '*' for an unbranded distribution removes just the
        # variants.
        dist_unbranded = model.Distribution(
            package_as_pkg=True, tagged_variants=['VAR1', 'VAR2']
        )
        filtered = pipeline._filter_distributions([dist_unbranded], ['*'], [])
        self.assertEqual(1, len(filtered))
        self.assertEqual((), filtered[0].tagged_variants)


@mock.patch.multiple(
    'signing.commands',
    **{
        m: mock.DEFAULT
        for m in (
            'move_file',
            'copy_files',
            'run_command',
            'run_command_all_output_async',
            'make_dir',
            'shutil',
            'os',
        )
    },
)
@mock.patch.multiple(
    'signing.notarize', **{m: mock.DEFAULT for m in ('submit', 'staple')}
)
@mock.patch.multiple(
    'signing.pipeline',
    **{
        m: mock.DEFAULT
        for m in (
            '_customize_and_sign_chrome',
            '_staple_chrome',
            '_package_and_sign_dmg',
            '_package_and_sign_pkg',
            '_package_zip',
        )
    },
)
@mock.patch('signing.commands.tempfile.mkdtemp', _get_work_dir)
class TestSignAll(unittest.TestCase):
    def setUp(self, **kwargs):
        _get_work_dir.count = 0
        self.paths = model.Paths('/$I', '/$O', None)

    def test_sign_basic_distribution_dmg(self, **kwargs):
        kwargs['run_command_all_output_async'].return_value = ('', 0, '', '')
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        kwargs[
            '_package_and_sign_dmg'
        ].return_value = '/$O/AppProduct-99.0.9999.99.dmg'

        class Config(test_config.TestConfig):
            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=True,
                        package_as_pkg=False,
                        package_as_zip=False,
                    ),
                ]

        config = Config()
        asyncio.run(pipeline.sign_all(self.paths, config))

        manager.assert_has_calls(
            [
                # First customize the distribution and sign it.
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$W_1/stable', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_2'),
                # Prepare the app for notarization.
                mock.call.run_command_all_output_async(
                    [
                        'zip',
                        '--recurse-paths',
                        '--symlinks',
                        '--quiet',
                        '/$W_1/AppProduct-99.0.9999.99.zip',
                        'App Product.app',
                    ],
                    cwd='/$W_1/stable',
                ),
                mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
                mock.call._staple_chrome(
                    self.paths.replace_work('/$W_1/stable'), mock.ANY
                ),
                # Make the DMG.
                mock.call._package_and_sign_dmg(mock.ANY, mock.ANY),
                # Notarize the DMG.
                mock.call.submit('/$O/AppProduct-99.0.9999.99.dmg', mock.ANY),
                mock.call.staple('/$O/AppProduct-99.0.9999.99.dmg'),
                mock.call.shutil.rmtree('/$W_1'),
            ]
        )

    def test_sign_basic_distribution_zip(self, **kwargs):
        kwargs['run_command_all_output_async'].return_value = ('', 0, '', '')
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        kwargs['_package_zip'].return_value = '/$O/AppProduct-99.0.9999.99.zip'

        class Config(test_config.TestConfig):
            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=False,
                        package_as_pkg=False,
                        package_as_zip=True,
                    ),
                ]

        config = Config()
        asyncio.run(pipeline.sign_all(self.paths, config))

        manager.assert_has_calls(
            [
                # First customize the distribution and sign it.
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$W_1/stable', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_2'),
                # Prepare the app for notarization.
                mock.call.run_command_all_output_async(
                    [
                        'zip',
                        '--recurse-paths',
                        '--symlinks',
                        '--quiet',
                        '/$W_1/AppProduct-99.0.9999.99.zip',
                        'App Product.app',
                    ],
                    cwd='/$W_1/stable',
                ),
                mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
                mock.call._staple_chrome(
                    self.paths.replace_work('/$W_1/stable'), mock.ANY
                ),
                # Make the ZIP.
                mock.call._package_zip(mock.ANY, mock.ANY),
                # Notarize the DMG.
                mock.call.shutil.rmtree('/$W_1'),
            ]
        )

    def test_sign_inflated_distribution_dmg(self, **kwargs):
        kwargs['run_command_all_output_async'].return_value = ('', 0, '', '')
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        kwargs[
            '_package_and_sign_dmg'
        ].return_value = '/$O/AppProduct-99.0.9999.99.dmg'

        class Config(test_config.TestConfig):
            @property
            def distributions(self):
                return [
                    model.Distribution(
                        inflation_kilobytes=5000,
                        package_as_dmg=True,
                        package_as_pkg=False,
                        package_as_zip=False,
                    ),
                ]

        config = Config()

        asyncio.run(pipeline.sign_all(self.paths, config))

        manager.assert_has_calls(
            [
                # First customize the distribution and sign it.
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$W_1/stable-5000', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_2'),
                # Prepare the app for notarization.
                mock.call.run_command_all_output_async(
                    [
                        'zip',
                        '--recurse-paths',
                        '--symlinks',
                        '--quiet',
                        '/$W_1/AppProduct-99.0.9999.99.zip',
                        'App Product.app',
                    ],
                    cwd='/$W_1/stable-5000',
                ),
                mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
                mock.call._staple_chrome(
                    self.paths.replace_work('/$W_1/stable-5000'), mock.ANY
                ),
                mock.call.run_command(
                    [
                        'dd',
                        'if=/dev/urandom',
                        'of=/$I/Product Packaging/inflation.bin',
                        'bs=1000',
                        'count=5000',
                    ]
                ),
                # Make the DMG.
                mock.call._package_and_sign_dmg(mock.ANY, mock.ANY),
                # Notarize the DMG.
                mock.call.submit('/$O/AppProduct-99.0.9999.99.dmg', mock.ANY),
                mock.call.staple('/$O/AppProduct-99.0.9999.99.dmg'),
                mock.call.shutil.rmtree('/$W_1'),
            ]
        )

    def test_sign_basic_distribution_pkg(self, **kwargs):
        kwargs['run_command_all_output_async'].return_value = ('', 0, '', '')
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        kwargs[
            '_package_and_sign_pkg'
        ].return_value = '/$O/AppProduct-99.0.9999.99.pkg'

        class Config(test_config.TestConfig):
            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=False,
                        package_as_pkg=True,
                        package_as_zip=False,
                    ),
                ]

        config = Config()
        asyncio.run(
            pipeline.sign_all(
                self.paths, config, tag_tool='/$I/Product Packaging/tag'
            )
        )

        manager.assert_has_calls(
            [
                # First customize the distribution and sign it.
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$W_1/stable', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_2'),
                # Prepare the app for notarization.
                mock.call.run_command_all_output_async(
                    [
                        'zip',
                        '--recurse-paths',
                        '--symlinks',
                        '--quiet',
                        '/$W_1/AppProduct-99.0.9999.99.zip',
                        'App Product.app',
                    ],
                    cwd='/$W_1/stable',
                ),
                mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
                mock.call._staple_chrome(
                    self.paths.replace_work('/$W_1/stable'), mock.ANY
                ),
                # Make the PKG.
                mock.call._package_and_sign_pkg(mock.ANY, mock.ANY),
                # Notarize the PKG.
                mock.call.submit('/$O/AppProduct-99.0.9999.99.pkg', mock.ANY),
                mock.call.staple('/$O/AppProduct-99.0.9999.99.pkg'),
                mock.call.shutil.rmtree('/$W_1'),
            ]
        )
        self.assertEqual(0, kwargs['run_command'].call_count)

    def test_sign_basic_distribution_pkg_tagging(self, **kwargs):
        kwargs['run_command_all_output_async'].return_value = ('', 0, '', '')
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        kwargs[
            '_package_and_sign_pkg'
        ].return_value = '/$O/AppProduct-99.0.9999.99.pkg'

        class Config(test_config.TestConfig):
            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=False,
                        package_as_pkg=True,
                        package_as_zip=False,
                        tagged_variants=[],
                    ),
                ]

        config = Config()
        tag_tool = '/$I/Product Packaging/tag'
        asyncio.run(pipeline.sign_all(self.paths, config, tag_tool=tag_tool))

        pkg_path = '/$O/AppProduct-99.0.9999.99.pkg'
        taggable_pkg_path = '/$O/AppProduct-99.0.9999.99-Unbranded-Taggable.pkg'
        staple_call = mock.call.staple(pkg_path)
        tag_call = mock.call.run_command(
            [tag_tool, '--set-tag', '--out=' + taggable_pkg_path, pkg_path]
        )
        verify_call = mock.call.run_command(
            ['pkgutil', '--check-signature', taggable_pkg_path]
        )
        staple_validate_call = mock.call.run_command(
            ['xcrun', 'stapler', 'validate', taggable_pkg_path]
        )

        self.assertIn(staple_call, manager.mock_calls)
        self.assertIn(tag_call, manager.mock_calls)
        self.assertIn(verify_call, manager.mock_calls)
        self.assertIn(staple_validate_call, manager.mock_calls)
        self.assertLess(
            manager.mock_calls.index(staple_call),
            manager.mock_calls.index(tag_call),
        )
        self.assertLess(
            manager.mock_calls.index(tag_call),
            manager.mock_calls.index(verify_call),
        )
        self.assertLess(
            manager.mock_calls.index(verify_call),
            manager.mock_calls.index(staple_validate_call),
        )
        self.assertEqual(
            [
                mock.call(
                    [
                        tag_tool,
                        '--set-tag',
                        '--out=' + taggable_pkg_path,
                        pkg_path,
                    ]
                ),
                mock.call(['pkgutil', '--check-signature', taggable_pkg_path]),
                mock.call(['xcrun', 'stapler', 'validate', taggable_pkg_path]),
            ],
            kwargs['run_command'].mock_calls,
        )

    def test_sign_basic_distribution_dmg_zip(self, **kwargs):
        kwargs['run_command_all_output_async'].return_value = ('', 0, '', '')
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        kwargs[
            '_package_and_sign_dmg'
        ].return_value = '/$O/AppProduct-99.0.9999.99.dmg'
        kwargs['_package_zip'].return_value = '/$O/AppProduct-99.0.9999.99.zip'

        class Config(test_config.TestConfig):
            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=True,
                        package_as_pkg=False,
                        package_as_zip=True,
                    ),
                ]

        config = Config()
        asyncio.run(pipeline.sign_all(self.paths, config))

        manager.assert_has_calls(
            [
                # First customize the distribution and sign it.
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$W_1/stable', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_2'),
                # Prepare the app for notarization.
                mock.call.run_command_all_output_async(
                    [
                        'zip',
                        '--recurse-paths',
                        '--symlinks',
                        '--quiet',
                        '/$W_1/AppProduct-99.0.9999.99.zip',
                        'App Product.app',
                    ],
                    cwd='/$W_1/stable',
                ),
                mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
                mock.call._staple_chrome(
                    self.paths.replace_work('/$W_1/stable'), mock.ANY
                ),
                # Make the DMG.
                mock.call._package_and_sign_dmg(mock.ANY, mock.ANY),
                mock.call.submit('/$O/AppProduct-99.0.9999.99.dmg', mock.ANY),
                # Make the ZIP.
                mock.call._package_zip(mock.ANY, mock.ANY),
                # Notarize the DMG.
                mock.call.staple('/$O/AppProduct-99.0.9999.99.dmg'),
                mock.call.shutil.rmtree('/$W_1'),
            ]
        )

    def test_sign_basic_distribution_dmg_pkg(self, **kwargs):
        kwargs['run_command_all_output_async'].return_value = ('', 0, '', '')
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        kwargs[
            '_package_and_sign_dmg'
        ].return_value = '/$O/AppProduct-99.0.9999.99.dmg'
        kwargs[
            '_package_and_sign_pkg'
        ].return_value = '/$O/AppProduct-99.0.9999.99.pkg'

        class Config(test_config.TestConfig):
            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=True,
                        package_as_pkg=True,
                        package_as_zip=False,
                    ),
                ]

        config = Config()
        asyncio.run(
            pipeline.sign_all(
                self.paths, config, tag_tool='/$I/Product Packaging/tag'
            )
        )

        manager.assert_has_calls(
            [
                # First customize the distribution and sign it.
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$W_1/stable', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_2'),
                # Prepare the app for notarization.
                mock.call.run_command_all_output_async(
                    [
                        'zip',
                        '--recurse-paths',
                        '--symlinks',
                        '--quiet',
                        '/$W_1/AppProduct-99.0.9999.99.zip',
                        'App Product.app',
                    ],
                    cwd='/$W_1/stable',
                ),
                mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
                mock.call._staple_chrome(
                    self.paths.replace_work('/$W_1/stable'), mock.ANY
                ),
                # Make the DMG, and submit for notarization.
                mock.call._package_and_sign_dmg(mock.ANY, mock.ANY),
                mock.call.submit('/$O/AppProduct-99.0.9999.99.dmg', mock.ANY),
                # Make the PKG, and submit for notarization.
                mock.call._package_and_sign_pkg(mock.ANY, mock.ANY),
                mock.call.submit('/$O/AppProduct-99.0.9999.99.pkg', mock.ANY),
                # Wait for notarization results.
                mock.call.staple('/$O/AppProduct-99.0.9999.99.dmg'),
                mock.call.staple('/$O/AppProduct-99.0.9999.99.pkg'),
                mock.call.shutil.rmtree('/$W_1'),
            ]
        )

    def test_sign_basic_distribution_dmg_pkg_tagging(self, **kwargs):
        kwargs['run_command_all_output_async'].return_value = ('', 0, '', '')
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        kwargs[
            '_package_and_sign_dmg'
        ].return_value = '/$O/AppProduct-99.0.9999.99.dmg'
        kwargs[
            '_package_and_sign_pkg'
        ].return_value = '/$O/AppProduct-99.0.9999.99.pkg'

        class Config(test_config.TestConfig):
            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=True,
                        package_as_pkg=True,
                        package_as_zip=False,
                        tagged_variants=[],
                    ),
                ]

        config = Config()
        tag_tool = '/$I/Product Packaging/tag'
        asyncio.run(pipeline.sign_all(self.paths, config, tag_tool=tag_tool))

        pkg_path = '/$O/AppProduct-99.0.9999.99.pkg'
        taggable_pkg_path = '/$O/AppProduct-99.0.9999.99-Unbranded-Taggable.pkg'
        staple_call = mock.call.staple(pkg_path)
        tag_call = mock.call.run_command(
            [tag_tool, '--set-tag', '--out=' + taggable_pkg_path, pkg_path]
        )
        verify_call = mock.call.run_command(
            ['pkgutil', '--check-signature', taggable_pkg_path]
        )
        staple_validate_call = mock.call.run_command(
            ['xcrun', 'stapler', 'validate', taggable_pkg_path]
        )

        self.assertIn(staple_call, manager.mock_calls)
        self.assertIn(tag_call, manager.mock_calls)
        self.assertIn(verify_call, manager.mock_calls)
        self.assertIn(staple_validate_call, manager.mock_calls)
        self.assertLess(
            manager.mock_calls.index(staple_call),
            manager.mock_calls.index(tag_call),
        )
        self.assertLess(
            manager.mock_calls.index(tag_call),
            manager.mock_calls.index(verify_call),
        )
        self.assertLess(
            manager.mock_calls.index(verify_call),
            manager.mock_calls.index(staple_validate_call),
        )
        self.assertEqual(
            [
                mock.call(
                    [
                        tag_tool,
                        '--set-tag',
                        '--out=' + taggable_pkg_path,
                        pkg_path,
                    ]
                ),
                mock.call(['pkgutil', '--check-signature', taggable_pkg_path]),
                mock.call(['xcrun', 'stapler', 'validate', taggable_pkg_path]),
            ],
            kwargs['run_command'].mock_calls,
        )

    def test_sign_basic_distribution_pkg_zip(self, **kwargs):
        kwargs['run_command_all_output_async'].return_value = ('', 0, '', '')
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        kwargs[
            '_package_and_sign_pkg'
        ].return_value = '/$O/AppProduct-99.0.9999.99.pkg'
        kwargs['_package_zip'].return_value = '/$O/AppProduct-99.0.9999.99.zip'

        class Config(test_config.TestConfig):
            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=False,
                        package_as_pkg=True,
                        package_as_zip=True,
                    ),
                ]

        config = Config()
        asyncio.run(
            pipeline.sign_all(
                self.paths, config, tag_tool='/$I/Product Packaging/tag'
            )
        )

        manager.assert_has_calls(
            [
                # First customize the distribution and sign it.
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$W_1/stable', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_2'),
                # Prepare the app for notarization.
                mock.call.run_command_all_output_async(
                    [
                        'zip',
                        '--recurse-paths',
                        '--symlinks',
                        '--quiet',
                        '/$W_1/AppProduct-99.0.9999.99.zip',
                        'App Product.app',
                    ],
                    cwd='/$W_1/stable',
                ),
                mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
                mock.call._staple_chrome(
                    self.paths.replace_work('/$W_1/stable'), mock.ANY
                ),
                # Make the PKG, and submit for notarization.
                mock.call._package_and_sign_pkg(mock.ANY, mock.ANY),
                mock.call.submit('/$O/AppProduct-99.0.9999.99.pkg', mock.ANY),
                # Make the ZIP.
                mock.call._package_zip(mock.ANY, mock.ANY),
                # Notarize the PKG.
                mock.call.staple('/$O/AppProduct-99.0.9999.99.pkg'),
                mock.call.shutil.rmtree('/$W_1'),
            ]
        )

    def test_sign_basic_distribution_dmg_pkg_zip(self, **kwargs):
        kwargs['run_command_all_output_async'].return_value = ('', 0, '', '')
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        kwargs[
            '_package_and_sign_dmg'
        ].return_value = '/$O/AppProduct-99.0.9999.99.dmg'
        kwargs[
            '_package_and_sign_pkg'
        ].return_value = '/$O/AppProduct-99.0.9999.99.pkg'
        kwargs['_package_zip'].return_value = '/$O/AppProduct-99.0.9999.99.zip'

        class Config(test_config.TestConfig):
            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=True,
                        package_as_pkg=True,
                        package_as_zip=True,
                    ),
                ]

        config = Config()
        asyncio.run(
            pipeline.sign_all(
                self.paths, config, tag_tool='/$I/Product Packaging/tag'
            )
        )

        manager.assert_has_calls(
            [
                # First customize the distribution and sign it.
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$W_1/stable', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_2'),
                # Prepare the app for notarization.
                mock.call.run_command_all_output_async(
                    [
                        'zip',
                        '--recurse-paths',
                        '--symlinks',
                        '--quiet',
                        '/$W_1/AppProduct-99.0.9999.99.zip',
                        'App Product.app',
                    ],
                    cwd='/$W_1/stable',
                ),
                mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
                mock.call._staple_chrome(
                    self.paths.replace_work('/$W_1/stable'), mock.ANY
                ),
                # Make the DMG.
                mock.call._package_and_sign_dmg(mock.ANY, mock.ANY),
                mock.call.submit('/$O/AppProduct-99.0.9999.99.dmg', mock.ANY),
                # Make the PKG, and submit for notarization.
                mock.call._package_and_sign_pkg(mock.ANY, mock.ANY),
                mock.call.submit('/$O/AppProduct-99.0.9999.99.pkg', mock.ANY),
                # Make the ZIP.
                mock.call._package_zip(mock.ANY, mock.ANY),
                # Notarize the DMG.
                mock.call.staple('/$O/AppProduct-99.0.9999.99.dmg'),
                mock.call.staple('/$O/AppProduct-99.0.9999.99.pkg'),
                mock.call.shutil.rmtree('/$W_1'),
            ]
        )

    def test_sign_no_packaging(self, **kwargs):
        kwargs['run_command_all_output_async'].return_value = ('', 0, '', '')
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig()
        asyncio.run(
            pipeline.sign_all(self.paths, config, disable_packaging=True)
        )

        manager.assert_has_calls(
            [
                # First customize the distribution and sign it.
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$W_1/stable', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_2'),
                # Prepare the app for notarization.
                mock.call.run_command_all_output_async(
                    [
                        'zip',
                        '--recurse-paths',
                        '--symlinks',
                        '--quiet',
                        '/$W_1/AppProduct-99.0.9999.99.zip',
                        'App Product.app',
                    ],
                    cwd='/$W_1/stable',
                ),
                mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
                mock.call._staple_chrome(
                    self.paths.replace_work('/$W_1/stable'), mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_1'),
            ]
        )

        self.assertEqual(1, kwargs['run_command_all_output_async'].call_count)

    def test_sign_notarize_wait_no_staple(self, **kwargs):
        kwargs['run_command_all_output_async'].return_value = ('', 0, '', '')
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        kwargs[
            '_package_and_sign_dmg'
        ].return_value = '/$O/AppProduct-99.0.9999.99.dmg'

        config = test_config.TestConfig(
            notarize=model.NotarizeAndStapleLevel.WAIT_NOSTAPLE
        )
        asyncio.run(pipeline.sign_all(self.paths, config))

        manager.assert_has_calls(
            [
                # First customize the distribution and sign it.
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$W_1/stable', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_2'),
                # Prepare the app for notarization.
                mock.call.run_command_all_output_async(
                    [
                        'zip',
                        '--recurse-paths',
                        '--symlinks',
                        '--quiet',
                        '/$W_1/AppProduct-99.0.9999.99.zip',
                        'App Product.app',
                    ],
                    cwd='/$W_1/stable',
                ),
                mock.call.submit('/$W_1/AppProduct-99.0.9999.99.zip', mock.ANY),
                # Make the DMG.
                mock.call._package_and_sign_dmg(mock.ANY, mock.ANY),
                # Notarize the DMG.
                mock.call.submit('/$O/AppProduct-99.0.9999.99.dmg', mock.ANY),
                # Cleanup.
                mock.call.shutil.rmtree('/$W_1'),
            ]
        )

    def test_sign_no_notarization(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig(
            notarize=model.NotarizeAndStapleLevel.NONE
        )
        asyncio.run(pipeline.sign_all(self.paths, config))

        manager.assert_has_calls(
            [
                # First customize the distribution and sign it.
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$W_1/stable', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_2'),
                # Make the DMG.
                mock.call._package_and_sign_dmg(mock.ANY, mock.ANY),
                # Cleanup.
                mock.call.shutil.rmtree('/$W_1'),
            ]
        )

    def test_sign_no_packaging_no_notarization(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig(
            notarize=model.NotarizeAndStapleLevel.NONE
        )
        asyncio.run(
            pipeline.sign_all(self.paths, config, disable_packaging=True)
        )

        manager.assert_has_calls(
            [
                # First customize the distribution and sign it.
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$O/stable', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_2'),
                mock.call.shutil.rmtree('/$W_1'),
            ]
        )

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
                        package_as_zip=False,
                    ),
                    model.Distribution(
                        branding_code='ARF',
                        packaging_name_fragment='ForDogs',
                        package_as_dmg=False,
                        package_as_pkg=True,
                        package_as_zip=False,
                    ),
                    model.Distribution(
                        branding_code='MEOW',
                        packaging_name_fragment='ForCats',
                        package_as_dmg=False,
                        package_as_pkg=False,
                        package_as_zip=True,
                    ),
                    model.Distribution(
                        branding_code='MOOF',
                        packaging_name_fragment='ForDogcows',
                        package_as_dmg=True,
                        package_as_pkg=True,
                        package_as_zip=False,
                    ),
                    model.Distribution(
                        branding_code='MEOARF',
                        packaging_name_fragment='ForCatdogs',
                        package_as_dmg=False,
                        package_as_pkg=True,
                        package_as_zip=True,
                    ),
                    model.Distribution(
                        branding_code='MOOEOW',
                        packaging_name_fragment='ForCowcats',
                        package_as_dmg=True,
                        package_as_pkg=False,
                        package_as_zip=True,
                    ),
                    model.Distribution(
                        branding_code='AHHHH',
                        packaging_name_fragment='ForCowdogcats',
                        package_as_dmg=True,
                        package_as_pkg=True,
                        package_as_zip=True,
                    ),
                ]

        config = Config(notarize=model.NotarizeAndStapleLevel.NONE)
        asyncio.run(
            pipeline.sign_all(
                self.paths, config, tag_tool='/$I/Product Packaging/tag'
            )
        )

        self.assertEqual(6, kwargs['_customize_and_sign_chrome'].call_count)

        manager.assert_has_calls(
            [
                # Customizations.
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$W_1/stable', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_2'),
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$W_1/stable-MOO', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_3'),
                mock.call.shutil.rmtree('/$W_4'),
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$W_1/stable-MEOW', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_5'),
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$W_1/stable-MOOF', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_6'),
                mock.call.shutil.rmtree('/$W_7'),
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$W_1/stable-MOOEOW', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_8'),
                mock.call._customize_and_sign_chrome(
                    mock.ANY, mock.ANY, '/$W_1/stable-AHHHH', mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_9'),
                # Packaging and signing.
                mock.call._package_and_sign_dmg(
                    self.paths.replace_work('/$W_1/stable'), mock.ANY
                ),
                mock.call._package_and_sign_dmg(
                    self.paths.replace_work('/$W_1/stable-MOO'), mock.ANY
                ),
                mock.call._package_and_sign_pkg(
                    self.paths.replace_work('/$W_1/stable'), mock.ANY
                ),
                mock.call._package_zip(
                    self.paths.replace_work('/$W_1/stable-MEOW'), mock.ANY
                ),
                mock.call._package_and_sign_dmg(
                    self.paths.replace_work('/$W_1/stable-MOOF'), mock.ANY
                ),
                mock.call._package_and_sign_pkg(
                    self.paths.replace_work('/$W_1/stable-MOOF'), mock.ANY
                ),
                mock.call._package_and_sign_pkg(
                    self.paths.replace_work('/$W_1/stable'), mock.ANY
                ),
                mock.call._package_zip(
                    self.paths.replace_work('/$W_1/stable'), mock.ANY
                ),
                mock.call._package_and_sign_dmg(
                    self.paths.replace_work('/$W_1/stable-MOOEOW'), mock.ANY
                ),
                mock.call._package_zip(
                    self.paths.replace_work('/$W_1/stable-MOOEOW'), mock.ANY
                ),
                mock.call._package_and_sign_dmg(
                    self.paths.replace_work('/$W_1/stable-AHHHH'), mock.ANY
                ),
                mock.call._package_and_sign_pkg(
                    self.paths.replace_work('/$W_1/stable-AHHHH'), mock.ANY
                ),
                mock.call._package_zip(
                    self.paths.replace_work('/$W_1/stable-AHHHH'), mock.ANY
                ),
                mock.call.shutil.rmtree('/$W_1'),
            ]
        )

    def test_sign_branded_distribution_tagging(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        kwargs['_package_and_sign_pkg'].side_effect = lambda paths, config: (
            '{}/{}.pkg'.format(paths.output, config.packaging_basename)
        )

        class Config(test_config.TestConfig):
            @property
            def distributions(self):
                return [
                    model.Distribution(
                        branding_code='GCEM',
                        packaging_name_fragment='Enterprise-Brand-GCEM',
                        package_as_dmg=False,
                        package_as_pkg=True,
                        package_as_zip=False,
                        tagged_variants=['GCSW', 'GCOA'],
                    ),
                    model.Distribution(
                        package_as_dmg=False,
                        package_as_pkg=True,
                        package_as_zip=False,
                        tagged_variants=[],
                    ),
                    model.Distribution(
                        branding_code='MOO',
                        packaging_name_fragment='ForCows',
                        package_as_dmg=False,
                        package_as_pkg=True,
                        package_as_zip=False,
                    ),
                ]

        config = Config(notarize=model.NotarizeAndStapleLevel.NONE)
        tag_tool = '/path/to/tag'
        asyncio.run(pipeline.sign_all(self.paths, config, tag_tool=tag_tool))

        self.assertEqual(1, kwargs['_customize_and_sign_chrome'].call_count)

        self.assertEqual(3, kwargs['_package_and_sign_pkg'].call_count)
        gcsw_path = (
            '/$O/AppProduct-99.0.9999.99-Enterprise-Brand-GCEM-Tagged-GCSW.pkg'
        )
        gcoa_path = (
            '/$O/AppProduct-99.0.9999.99-Enterprise-Brand-GCEM-Tagged-GCOA.pkg'
        )
        unbranded_taggable_path = (
            '/$O/AppProduct-99.0.9999.99-Unbranded-Taggable.pkg'
        )
        taggable_path = (
            '/$O/AppProduct-99.0.9999.99-Enterprise-Brand-GCEM-Taggable.pkg'
        )
        self.assertEqual(
            [
                mock.call(
                    [
                        tag_tool,
                        '--set-tag',
                        '--out=' + taggable_path,
                        (
                            '/$O/'
                            'AppProduct-99.0.9999.99-Enterprise-Brand-GCEM.pkg'
                        ),
                    ]
                ),
                mock.call(['pkgutil', '--check-signature', taggable_path]),
                mock.call(
                    [
                        tag_tool,
                        '--set-tag=brand=GCSW',
                        '--out=' + gcsw_path,
                        taggable_path,
                    ]
                ),
                mock.call(
                    [
                        'pkgutil',
                        '--check-signature',
                        gcsw_path,
                    ]
                ),
                mock.call(
                    [
                        tag_tool,
                        '--set-tag=brand=GCOA',
                        '--out=' + gcoa_path,
                        taggable_path,
                    ]
                ),
                mock.call(
                    [
                        'pkgutil',
                        '--check-signature',
                        gcoa_path,
                    ]
                ),
                mock.call(
                    [
                        tag_tool,
                        '--set-tag',
                        '--out=' + unbranded_taggable_path,
                        '/$O/AppProduct-99.0.9999.99.pkg',
                    ]
                ),
                mock.call(
                    [
                        'pkgutil',
                        '--check-signature',
                        unbranded_taggable_path,
                    ]
                ),
            ],
            kwargs['run_command'].mock_calls,
        )

    def test_sign_all_pkg_requires_tag_tool(self, **kwargs):
        class Config(test_config.TestConfig):
            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=False,
                        package_as_pkg=True,
                        package_as_zip=False,
                        tagged_variants=[],
                    ),
                ]

        config = Config(notarize=model.NotarizeAndStapleLevel.NONE)
        with self.assertRaises(ValueError) as cm:
            asyncio.run(pipeline.sign_all(self.paths, config, tag_tool=None))
        self.assertEqual(
            'tag_tool must be specified when a distribution '
            'has tagged_variants',
            str(cm.exception),
        )

    def test_sign_all_pkg_no_tagged_variants_no_tag_tool(self, **kwargs):
        class Config(test_config.TestConfig):
            @property
            def distributions(self):
                return [
                    model.Distribution(
                        package_as_dmg=False,
                        package_as_pkg=True,
                        package_as_zip=False,
                        tagged_variants=None,
                    ),
                ]

        config = Config(notarize=model.NotarizeAndStapleLevel.NONE)
        asyncio.run(pipeline.sign_all(self.paths, config, tag_tool=None))

    def test_sign_tagged_variants_skip_brands(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        kwargs['_package_and_sign_pkg'].side_effect = lambda paths, config: (
            '{}/{}.pkg'.format(paths.output, config.packaging_basename)
        )

        class Config(test_config.TestConfig):
            @property
            def distributions(self):
                return [
                    model.Distribution(
                        branding_code='GCEM',
                        packaging_name_fragment='Enterprise-Brand-GCEM',
                        package_as_dmg=False,
                        package_as_pkg=True,
                        package_as_zip=False,
                        tagged_variants=['GCSW', 'GCOA'],
                    ),
                ]

        config = Config(notarize=model.NotarizeAndStapleLevel.NONE)
        tag_tool = '/path/to/tag'
        asyncio.run(
            pipeline.sign_all(
                self.paths, config, tag_tool=tag_tool, skip_brands=['GCOA']
            )
        )

        gcsw_path = (
            '/$O/AppProduct-99.0.9999.99-Enterprise-Brand-GCEM-Tagged-GCSW.pkg'
        )
        taggable_path = (
            '/$O/AppProduct-99.0.9999.99-Enterprise-Brand-GCEM-Taggable.pkg'
        )
        self.assertEqual(
            [
                mock.call(
                    [
                        tag_tool,
                        '--set-tag',
                        '--out=' + taggable_path,
                        (
                            '/$O/'
                            'AppProduct-99.0.9999.99-Enterprise-Brand-GCEM.pkg'
                        ),
                    ]
                ),
                mock.call(['pkgutil', '--check-signature', taggable_path]),
                mock.call(
                    [
                        tag_tool,
                        '--set-tag=brand=GCSW',
                        '--out=' + gcsw_path,
                        taggable_path,
                    ]
                ),
                mock.call(
                    [
                        'pkgutil',
                        '--check-signature',
                        gcsw_path,
                    ]
                ),
            ],
            kwargs['run_command'].mock_calls,
        )

    def test_sign_tagged_variants_own_brand_variant(self, **kwargs):
        dist = model.Distribution(
            branding_code='GCEM',
            package_as_dmg=False,
            package_as_pkg=True,
            package_as_zip=False,
            tagged_variants=['GCEM'],
        )

        class Config(test_config.TestConfig):
            @property
            def distributions(self):
                return [dist]

        config = Config(notarize=model.NotarizeAndStapleLevel.NONE)
        asyncio.run(
            pipeline.sign_all(self.paths, config, tag_tool='/path/to/tag')
        )

    def test_sign_tagged_variants_path_collision(self, **kwargs):
        class Config(test_config.TestConfig):
            @property
            def distributions(self):
                return [
                    model.Distribution(
                        packaging_name_fragment='Unbranded-Tagged-GCSW',
                        package_as_dmg=False,
                        package_as_pkg=True,
                        package_as_zip=False,
                    ),
                    model.Distribution(
                        package_as_dmg=False,
                        package_as_pkg=True,
                        package_as_zip=False,
                        tagged_variants=['GCSW'],
                    ),
                ]

        config = Config(notarize=model.NotarizeAndStapleLevel.NONE)
        with self.assertRaises(ValueError) as cm:
            asyncio.run(
                pipeline.sign_all(self.paths, config, tag_tool='/path/to/tag')
            )
        self.assertEqual(
            "Duplicate package output path for tagged variant 'GCSW': "
            '/$O/AppProduct-99.0.9999.99-Unbranded-Tagged-GCSW.pkg',
            str(cm.exception),
        )

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
        asyncio.run(
            pipeline.sign_all(
                self.paths,
                config,
                skip_brands=skip_brands,
                channels=include_channels,
            )
        )

        self.assertEqual(_last_brand_filter(), skip_brands)
        self.assertEqual(_last_channel_filter(), include_channels)
