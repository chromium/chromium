# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock

from signing import model, signing, test_config


@mock.patch('signing.commands.run_command_output')
class TestBinaryArchitecturesOffsets(unittest.TestCase):

    def test_none(self, run_command_output):
        run_command_output.return_value = b''
        self.assertRaises(AttributeError,
                          lambda: signing._binary_architectures_offsets(None))
        run_command_output.assert_called_once()

    def test_non_universal(self, run_command_output):
        run_command_output.return_value = b'''input file Test.app/Contents/MacOS/Test is not a fat file
Non-fat file: Test.app/Contents/MacOS/Test is architecture: x86_64
'''
        self.assertEquals(
            signing._binary_architectures_offsets(None), (('x86_64', 0),))
        run_command_output.assert_called_once()

    def test_universal(self, run_command_output):
        run_command_output.return_value = b'''Fat header in: Test.app/Contents/MacOS/Test
fat_magic 0xcafebabe
nfat_arch 2
architecture x86_64
    cputype CPU_TYPE_X86_64
    cpusubtype CPU_SUBTYPE_X86_64_ALL
    capabilities CPU_SUBTYPE_LIB64
    offset 16384
    size 274080
    align 2^14 (16384)
architecture arm64
    cputype CPU_TYPE_ARM64
    cpusubtype CPU_SUBTYPE_ARM64_ALL
    capabilities 0x0
    offset 294912
    size 208032
    align 2^14 (16384)
    '''
        self.assertEquals(
            signing._binary_architectures_offsets(None), (('x86_64', 16384),
                                                          ('arm64', 294912)))
        run_command_output.assert_called_once()

    def test_universal_invalid_arch_count(self, run_command_output):
        run_command_output.return_value = b'''Fat header in: Test.app/Contents/MacOS/Test
fat_magic 0xcafebabe
nfat_arch 2
architecture x86_64
    cputype CPU_TYPE_X86_64
    cpusubtype CPU_SUBTYPE_X86_64_ALL
    capabilities CPU_SUBTYPE_LIB64
    offset 16384
    size 274080
    align 2^14 (16384)
    '''
        self.assertRaises(signing.InvalidLipoArchCountException,
                          lambda: signing._binary_architectures_offsets(None))
        run_command_output.assert_called_once()


@mock.patch('signing.commands.run_command_output')
class TestValidateAppGeometry(unittest.TestCase):

    def setUp(self):
        self.paths = model.Paths('/$I', '/$O', '/$W')
        self.part = model.CodeSignedProduct('Test.app', 'test.signing.app')

    def config_with_pinned_geometry(self, geometry):

        class Config(test_config.TestConfig):

            @property
            def main_executable_pinned_geometry(self):
                return geometry

        return model.Distribution().to_config(Config())

    def test_none(self, run_command_output):
        config = self.config_with_pinned_geometry(None)
        self.assertEquals(
            signing.validate_app_geometry(self.paths, config, self.part), None)
        run_command_output.assert_not_called()

    def test_valid(self, run_command_output):
        run_command_output.return_value = b'''Fat header in: Test.app/Contents/MacOS/Test
fat_magic 0xcafebabe
nfat_arch 2
architecture x86_64
    cputype CPU_TYPE_X86_64
    cpusubtype CPU_SUBTYPE_X86_64_ALL
    capabilities CPU_SUBTYPE_LIB64
    offset 16384
    size 274080
    align 2^14 (16384)
architecture arm64
    cputype CPU_TYPE_ARM64
    cpusubtype CPU_SUBTYPE_ARM64_ALL
    capabilities 0x0
    offset 294912
    size 208032
    align 2^14 (16384)
    '''
        pinned_geometry = (('x86_64', 16384), ('arm64', 294912))
        config = self.config_with_pinned_geometry(pinned_geometry)
        self.assertEquals(
            signing.validate_app_geometry(self.paths, config, self.part), None)
        run_command_output.assert_called_once()

    def test_invalid(self, run_command_output):
        run_command_output.return_value = b'''Fat header in: Test.app/Contents/MacOS/Test
fat_magic 0xcafebabe
nfat_arch 2
architecture x86_64
    cputype CPU_TYPE_X86_64
    cpusubtype CPU_SUBTYPE_X86_64_ALL
    capabilities CPU_SUBTYPE_LIB64
    offset 16384
    size 274080
    align 2^14 (16384)
architecture arm64
    cputype CPU_TYPE_ARM64
    cpusubtype CPU_SUBTYPE_ARM64_ALL
    capabilities 0x0
    offset 294912
    size 208032
    align 2^14 (16384)
    '''
        pinned_geometry = (('x86_64', 16384), ('arm64', 999999))
        config = self.config_with_pinned_geometry(pinned_geometry)
        self.assertRaises(
            signing.InvalidAppGeometryException, lambda: signing.
            validate_app_geometry(self.paths, config, self.part))
        run_command_output.assert_called_once()


@mock.patch('signing.commands.run_command')
class TestSignPart(unittest.TestCase):

    def setUp(self):
        self.paths = model.Paths('/$I', '/$O', '/$W')
        self.config = test_config.TestConfig()

    def test_sign_part(self, run_command):
        part = model.CodeSignedProduct('Test.app', 'test.signing.app')
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with(
            ['codesign', '--sign', '[IDENTITY]', '--timestamp', '/$W/Test.app'])

    def test_sign_part_with_requirements(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app', 'test.signing.app', requirements='and true')
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--timestamp', '--requirements',
            '=designated => identifier "test.signing.app" and true',
            '/$W/Test.app'
        ])

    def test_sign_part_no_notary(self, run_command):
        config = test_config.TestConfig(
            notarize=model.NotarizeAndStapleLevel.NONE)
        part = model.CodeSignedProduct('Test.app', 'test.signing.app')
        signing.sign_part(self.paths, config, part)
        run_command.assert_called_once_with(
            ['codesign', '--sign', '[IDENTITY]', '/$W/Test.app'])

    def test_sign_part_no_identifier_requirement(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app', 'test.signing.app', identifier_requirement=False)
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with(
            ['codesign', '--sign', '[IDENTITY]', '--timestamp', '/$W/Test.app'])

    def test_sign_with_identifier(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app', 'test.signing.app', sign_with_identifier=True)
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--timestamp', '--identifier',
            'test.signing.app', '/$W/Test.app'
        ])

    def test_sign_with_requirement_and_identifier(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app',
            'test.signing.app',
            requirements='and true',
            sign_with_identifier=True)
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--timestamp', '--identifier',
            'test.signing.app', '--requirements',
            '=designated => identifier "test.signing.app" and true',
            '/$W/Test.app'
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
            'test.signing.app', '/$W/Test.app'
        ])

    def test_sign_part_with_options(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app',
            'test.signing.app',
            options=model.CodeSignOptions.RESTRICT
            | model.CodeSignOptions.LIBRARY_VALIDATION)
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--timestamp', '--options',
            'library,restrict', '/$W/Test.app'
        ])

    def test_sign_part_with_requirement_and_options(self, run_command):
        config = test_config.TestConfig(codesign_requirements_basic='or false')
        part = model.CodeSignedProduct(
            'Test.app',
            'test.signing.app',
            options=model.CodeSignOptions.RESTRICT
            | model.CodeSignOptions.LIBRARY_VALIDATION)
        signing.sign_part(self.paths, config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--timestamp', '--requirements',
            '=designated => identifier "test.signing.app" or false',
            '--options', 'library,restrict', '/$W/Test.app'
        ])

    def test_sign_part_with_requirements_and_options(self, run_command):
        config = test_config.TestConfig(codesign_requirements_basic='or false')
        part = model.CodeSignedProduct(
            'Test.app',
            'test.signing.app',
            requirements='and true',
            options=model.CodeSignOptions.RESTRICT
            | model.CodeSignOptions.LIBRARY_VALIDATION)
        signing.sign_part(self.paths, config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--timestamp', '--requirements',
            '=designated => identifier "test.signing.app" and true or false',
            '--options', 'library,restrict', '/$W/Test.app'
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
            '/$W/entitlements.plist', '/$W/Test.app'
        ])

    def test_verify_part(self, run_command):
        part = model.CodeSignedProduct('Test.app', 'test.signing.app')
        signing.verify_part(self.paths, part)
        self.assertEqual(run_command.mock_calls, [
            mock.call([
                'codesign', '--display', '--verbose=5', '--requirements', '-',
                '/$W/Test.app'
            ]),
            mock.call(['codesign', '--verify', '--verbose=6', '/$W/Test.app']),
        ])

    def test_verify_part_with_options(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app',
            'test.signing.app',
            verify_options=model.VerifyOptions.DEEP
            | model.VerifyOptions.IGNORE_RESOURCES)
        signing.verify_part(self.paths, part)
        self.assertEqual(run_command.mock_calls, [
            mock.call([
                'codesign', '--display', '--verbose=5', '--requirements', '-',
                '/$W/Test.app'
            ]),
            mock.call([
                'codesign', '--verify', '--verbose=6', '--deep',
                '--ignore-resources', '/$W/Test.app'
            ]),
        ])
