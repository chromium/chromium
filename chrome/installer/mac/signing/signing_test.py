# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from . import model, signing, test_common, test_config

mock = test_common.import_mock()

# python2 support.
try:
    FileNotFoundError
except NameError:
    FileNotFoundError = IOError


class TestGetParts(unittest.TestCase):

    def test_get_parts_no_base(self):
        config = test_config.TestConfig()
        all_parts = signing.get_parts(config)
        self.assertEqual('test.signing.bundle_id', all_parts['app'].identifier)
        self.assertEqual('test.signing.bundle_id.framework',
                         all_parts['framework'].identifier)
        self.assertEqual(
            'test.signing.bundle_id.framework.AlertNotificationService',
            all_parts['notification-xpc'].identifier)
        self.assertEqual('test.signing.bundle_id.helper',
                         all_parts['helper-app'].identifier)

    def test_get_parts_no_customize(self):
        config = model.Distribution(channel='dev').to_config(
            test_config.TestConfig())
        all_parts = signing.get_parts(config)
        self.assertEqual('test.signing.bundle_id', all_parts['app'].identifier)
        self.assertEqual('test.signing.bundle_id.framework',
                         all_parts['framework'].identifier)
        self.assertEqual(
            'test.signing.bundle_id.framework.AlertNotificationService',
            all_parts['notification-xpc'].identifier)
        self.assertEqual('test.signing.bundle_id.helper',
                         all_parts['helper-app'].identifier)

    def test_get_parts_customize(self):
        config = model.Distribution(
            channel='canary',
            channel_customize=True).to_config(test_config.TestConfig())
        all_parts = signing.get_parts(config)
        self.assertEqual('test.signing.bundle_id.canary',
                         all_parts['app'].identifier)
        self.assertEqual('test.signing.bundle_id.framework',
                         all_parts['framework'].identifier)
        self.assertEqual(
            'test.signing.bundle_id.canary.framework.AlertNotificationService',
            all_parts['notification-xpc'].identifier)
        self.assertEqual('test.signing.bundle_id.helper',
                         all_parts['helper-app'].identifier)

    def test_part_options(self):
        parts = signing.get_parts(test_config.TestConfig())
        self.assertEqual(
            set(model.CodeSignOptions.RESTRICT +
                model.CodeSignOptions.LIBRARY_VALIDATION +
                model.CodeSignOptions.KILL +
                model.CodeSignOptions.HARDENED_RUNTIME),
            set(parts['app'].options))
        self.assertEqual(
            set(model.CodeSignOptions.RESTRICT +
                model.CodeSignOptions.LIBRARY_VALIDATION +
                model.CodeSignOptions.KILL +
                model.CodeSignOptions.HARDENED_RUNTIME),
            set(parts['helper-app'].options))
        self.assertEqual(
            set(model.CodeSignOptions.RESTRICT + model.CodeSignOptions.KILL +
                model.CodeSignOptions.HARDENED_RUNTIME),
            set(parts['helper-renderer-app'].options))
        self.assertEqual(
            set(model.CodeSignOptions.RESTRICT + model.CodeSignOptions.KILL +
                model.CodeSignOptions.HARDENED_RUNTIME),
            set(parts['helper-gpu-app'].options))
        self.assertEqual(
            set(model.CodeSignOptions.RESTRICT + model.CodeSignOptions.KILL +
                model.CodeSignOptions.HARDENED_RUNTIME),
            set(parts['helper-plugin-app'].options))
        self.assertEqual(
            set(model.CodeSignOptions.RESTRICT +
                model.CodeSignOptions.LIBRARY_VALIDATION +
                model.CodeSignOptions.KILL +
                model.CodeSignOptions.HARDENED_RUNTIME),
            set(parts['crashpad'].options))
        self.assertEqual(
            set(model.CodeSignOptions.RESTRICT +
                model.CodeSignOptions.LIBRARY_VALIDATION +
                model.CodeSignOptions.KILL +
                model.CodeSignOptions.HARDENED_RUNTIME),
            set(parts['notification-xpc'].options))
        self.assertEqual(
            set(model.CodeSignOptions.RESTRICT +
                model.CodeSignOptions.LIBRARY_VALIDATION +
                model.CodeSignOptions.KILL +
                model.CodeSignOptions.HARDENED_RUNTIME),
            set(parts['app-mode-app'].options))


@mock.patch('signing.commands.run_command')
class TestSignPart(unittest.TestCase):

    def setUp(self):
        self.paths = model.Paths('$I', '$O', '$W')
        self.config = test_config.TestConfig()

    def test_sign_part(self, run_command):
        part = model.CodeSignedProduct('Test.app', 'test.signing.app')
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--timestamp', '--requirements',
            '=designated => identifier "test.signing.app"', '$W/Test.app'
        ])

    def test_sign_part_no_notary(self, run_command):
        config = test_config.TestConfig(notary_user=None, notary_password=None)
        part = model.CodeSignedProduct('Test.app', 'test.signing.app')
        signing.sign_part(self.paths, config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--requirements',
            '=designated => identifier "test.signing.app"', '$W/Test.app'
        ])

    def test_sign_part_no_identifier_requirement(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app', 'test.signing.app', identifier_requirement=False)
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with(
            ['codesign', '--sign', '[IDENTITY]', '--timestamp', '$W/Test.app'])

    def test_sign_with_identifier(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app', 'test.signing.app', sign_with_identifier=True)
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--timestamp', '--identifier',
            'test.signing.app', '--requirements',
            '=designated => identifier "test.signing.app"', '$W/Test.app'
        ])

    def test_sign_with_identifier_no_requirement(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app',
            'test.signing.app',
            sign_with_identifier=True,
            identifier_requirement=False)
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--timestamp', '--identifier',
            'test.signing.app', '$W/Test.app'
        ])

    def test_sign_part_with_options(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app',
            'test.signing.app',
            options=model.CodeSignOptions.RESTRICT +
            model.CodeSignOptions.LIBRARY_VALIDATION)
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--timestamp', '--requirements',
            '=designated => identifier "test.signing.app"', '--options',
            'restrict,library', '$W/Test.app'
        ])

    def test_sign_part_with_entitlements(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app',
            'test.signing.app',
            entitlements='entitlements.plist',
            identifier_requirement=False)
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--timestamp', '--entitlements',
            '$W/entitlements.plist', '$W/Test.app'
        ])

    def test_verify_part(self, run_command):
        part = model.CodeSignedProduct('Test.app', 'test.signing.app')
        signing.verify_part(self.paths, part)
        self.assertEqual(run_command.mock_calls, [
            mock.call([
                'codesign', '--display', '--verbose=5', '--requirements', '-',
                '$W/Test.app'
            ]),
            mock.call(['codesign', '--verify', '--verbose=6', '$W/Test.app']),
        ])

    def test_verify_part_with_options(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app',
            'test.signing.app',
            verify_options=model.VerifyOptions.DEEP +
            model.VerifyOptions.IGNORE_RESOURCES)
        signing.verify_part(self.paths, part)
        self.assertEqual(run_command.mock_calls, [
            mock.call([
                'codesign', '--display', '--verbose=5', '--requirements', '-',
                '$W/Test.app'
            ]),
            mock.call([
                'codesign', '--verify', '--verbose=6', '--deep',
                '--ignore-resources', '$W/Test.app'
            ]),
        ])


def _get_plist_read(other_version):

    def _plist_read(*args):
        path = args[0]
        first_slash = path.find('/')
        path = path[first_slash + 1:]

        plists = {
            'App Product.app/Contents/Info.plist': {
                'KSVersion': '99.0.9999.99'
            },
            'App Product.app/Contents/Frameworks/Product Framework.framework/Resources/Info.plist':
                {
                    'CFBundleShortVersionString': other_version
                }
        }
        return plists[path]

    return _plist_read


@mock.patch.multiple('signing.signing',
                     **{m: mock.DEFAULT for m in ('sign_part', 'verify_part')})
@mock.patch.multiple('signing.commands', **{
    m: mock.DEFAULT
    for m in ('copy_files', 'move_file', 'make_dir', 'run_command')
})
class TestSignChrome(unittest.TestCase):

    def setUp(self):
        self.paths = model.Paths('$I', '$O', '$W')

    @mock.patch('signing.signing._sanity_check_version_keys')
    def test_sign_chrome(self, *args, **kwargs):
        manager = mock.Mock()
        for kwarg in kwargs:
            manager.attach_mock(kwargs[kwarg], kwarg)

        dist = model.Distribution()
        config = dist.to_config(test_config.TestConfig())

        signing.sign_chrome(self.paths, config, sign_framework=True)

        # No files should be moved.
        self.assertEqual(0, kwargs['move_file'].call_count)

        # Test that the provisioning profile is copied.
        self.assertEqual(kwargs['copy_files'].mock_calls, [
            mock.call.copy_files(
                '$I/Product Packaging/provisiontest.provisionprofile',
                '$W/App Product.app/Contents/embedded.provisionprofile')
        ])

        # Ensure that all the parts are signed.
        signed_paths = [
            call[1][2].path for call in kwargs['sign_part'].mock_calls
        ]
        self.assertEqual(
            set([p.path for p in signing.get_parts(config).values()]),
            set(signed_paths))

        # Make sure that the framework and the app are the last two parts that
        # are signed.
        self.assertEqual(signed_paths[-2:], [
            'App Product.app/Contents/Frameworks/Product Framework.framework',
            'App Product.app'
        ])

        self.assertEqual(kwargs['run_command'].mock_calls, [
            mock.call.run_command([
                'codesign', '--display', '--requirements', '-', '--verbose=5',
                '$W/App Product.app'
            ]),
            mock.call.run_command(
                ['spctl', '--assess', '-vv', '$W/App Product.app']),
        ])

    @mock.patch('signing.signing._sanity_check_version_keys')
    def test_sign_chrome_no_assess(self, *args, **kwargs):
        dist = model.Distribution()

        class Config(test_config.TestConfig):

            @property
            def run_spctl_assess(self):
                return False

        config = dist.to_config(Config())

        signing.sign_chrome(self.paths, config, sign_framework=True)

        self.assertEqual(kwargs['run_command'].mock_calls, [
            mock.call.run_command([
                'codesign', '--display', '--requirements', '-', '--verbose=5',
                '$W/App Product.app'
            ]),
        ])

    @mock.patch('signing.signing._sanity_check_version_keys')
    def test_sign_chrome_no_provisioning(self, *args, **kwargs):
        dist = model.Distribution()

        class Config(test_config.TestConfig):

            @property
            def provisioning_profile_basename(self):
                return None

        config = dist.to_config(Config())
        signing.sign_chrome(self.paths, config, sign_framework=True)

        self.assertEqual(0, kwargs['copy_files'].call_count)

    @mock.patch('signing.signing._sanity_check_version_keys')
    def test_sign_chrome_optional_parts(self, *args, **kwargs):

        def _fail_to_sign(*args):
            if args[2].identifier == 'libwidevinecdm':
                raise FileNotFoundError(args[2].path)

        def _file_exists(*args):
            return not args[0].endswith('libwidevinecdm.dylib')

        kwargs['sign_part'].side_effect = _fail_to_sign

        with mock.patch(
                'signing.commands.file_exists', side_effect=_file_exists):
            # If the file is missing, signing should fail since TestConfig has
            # no optional parts.
            config = model.Distribution().to_config(test_config.TestConfig())
            self.assertRaises(
                FileNotFoundError,
                lambda: signing.sign_chrome(self.paths, config, sign_framework=True))

            class Config(test_config.TestConfig):

                @property
                def optional_parts(self):
                    return set(('libwidevinecdm.dylib',))

            # With the part marked as optional, it should succeed.
            config = model.Distribution().to_config(Config())
            signing.sign_chrome(self.paths, config, sign_framework=True)

    @mock.patch('signing.signing._sanity_check_version_keys')
    def test_sign_chrome_no_framework(self, *args, **kwargs):
        manager = mock.Mock()
        for kwarg in kwargs:
            manager.attach_mock(kwargs[kwarg], kwarg)

        dist = model.Distribution()
        config = dist.to_config(test_config.TestConfig())

        signing.sign_chrome(self.paths, config, sign_framework=False)

        # No files should be moved.
        self.assertEqual(0, kwargs['move_file'].call_count)

        # Test that the provisioning profile is copied.
        self.assertEqual(kwargs['copy_files'].mock_calls, [
            mock.call.copy_files(
                '$I/Product Packaging/provisiontest.provisionprofile',
                '$W/App Product.app/Contents/embedded.provisionprofile')
        ])

        # Ensure that only the app is signed.
        signed_paths = [
            call[1][2].path for call in kwargs['sign_part'].mock_calls
        ]
        self.assertEqual(signed_paths, ['App Product.app'])

        self.assertEqual(kwargs['run_command'].mock_calls, [
            mock.call.run_command([
                'codesign', '--display', '--requirements', '-', '--verbose=5',
                '$W/App Product.app'
            ]),
            mock.call.run_command(
                ['spctl', '--assess', '-vv', '$W/App Product.app']),
        ])

    @mock.patch(
        'signing.commands.plistlib.readPlist',
        side_effect=_get_plist_read('99.0.9999.99'))
    def test_sanity_check_ok(self, read_plist, **kwargs):
        config = model.Distribution().to_config(test_config.TestConfig())
        signing.sign_chrome(self.paths, config, sign_framework=True)

    @mock.patch(
        'signing.commands.plistlib.readPlist',
        side_effect=_get_plist_read('55.0.5555.55'))
    def test_sanity_check_bad(self, read_plist, **kwargs):
        config = model.Distribution().to_config(test_config.TestConfig())
        self.assertRaises(ValueError,
                          lambda: signing.sign_chrome(self.paths, config, sign_framework=True))
