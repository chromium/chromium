# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from . import model
from .test_config import TestConfig


class TestCodeSignedProduct(unittest.TestCase):

    def test_requirements_string_identifier(self):
        product = model.CodeSignedProduct('path/binary', 'binary')
        self.assertEqual('designated => identifier "binary"',
                         product.requirements_string(TestConfig()))

    def test_requirements_no_identifier(self):
        product = model.CodeSignedProduct(
            'path/binary', 'binary', identifier_requirement=False)
        self.assertEqual('', product.requirements_string(TestConfig()))

    def test_requirements_string_ad_hoc(self):
        config = TestConfig(identity='-')
        product = model.CodeSignedProduct('path/binary', 'binary')
        self.assertEqual('', product.requirements_string(config))

        product = model.CodeSignedProduct(
            'path/binary', 'binary', requirements='req')
        self.assertEqual('', product.requirements_string(config))

    def test_requirements_product_requirement(self):
        product = model.CodeSignedProduct(
            'path/binary', 'binary', requirements='and another requirement')
        self.assertEqual(
            'designated => identifier "binary" and another requirement',
            product.requirements_string(TestConfig()))

    def test_requirements_config_requirement(self):

        class RequirementConfig(TestConfig):

            @property
            def codesign_requirements_basic(self):
                return 'and config requirement'

        product = model.CodeSignedProduct(
            'path/binary', 'binary', requirements='and another requirement')
        self.assertEqual(
            'designated => identifier "binary" and another requirement and config requirement',
            product.requirements_string(RequirementConfig()))

        self.assertEqual(
            '', product.requirements_string(RequirementConfig(identity='-')))


class TestVerifyOptions(unittest.TestCase):

    def test_valid_all(self):
        opts = (
            model.VerifyOptions.DEEP + model.VerifyOptions.NO_STRICT +
            model.VerifyOptions.IGNORE_RESOURCES)
        self.assertTrue(model.VerifyOptions.valid(opts))

    def test_invalid(self):
        self.assertFalse(model.VerifyOptions.valid(['--whatever']))


class TestDistribution(unittest.TestCase):

    def test_no_options(self):
        base_config = TestConfig()
        config = model.Distribution().to_config(base_config)
        self.assertEqual(base_config, config.base_config)
        self.assertEqual(base_config.app_product, config.app_product)
        self.assertEqual(base_config.base_bundle_id, config.base_bundle_id)
        self.assertEqual(base_config.provisioning_profile_basename,
                         config.provisioning_profile_basename)
        self.assertEqual(base_config.packaging_basename,
                         config.packaging_basename)

    def test_channel_no_customize(self):
        base_config = TestConfig()
        config = model.Distribution(
            channel='Beta', app_name_fragment='Beta').to_config(base_config)
        self.assertEqual(base_config.app_product, config.app_product)
        self.assertEqual(base_config.product, config.product)
        self.assertEqual(base_config.base_bundle_id, config.base_bundle_id)
        self.assertEqual(base_config.provisioning_profile_basename,
                         config.provisioning_profile_basename)
        self.assertEqual(base_config.packaging_basename,
                         config.packaging_basename)

    def test_channel_customize(self):
        base_config = TestConfig()
        config = model.Distribution(
            channel='beta', app_name_fragment='Beta',
            channel_customize=True).to_config(base_config)
        self.assertEqual('App Product Beta', config.app_product)
        self.assertEqual(base_config.product, config.product)
        self.assertEqual('test.signing.bundle_id.beta', config.base_bundle_id)
        self.assertEqual('provisiontest_Beta',
                         config.provisioning_profile_basename)
        self.assertEqual('AppProductBeta-99.0.9999.99',
                         config.packaging_basename)

    def test_packaging_basename_no_channel(self):
        base_config = TestConfig()
        config = model.Distribution(
            packaging_name_fragment='Canary').to_config(base_config)
        self.assertEqual(base_config, config.base_config)
        self.assertEqual(base_config.app_product, config.app_product)
        self.assertEqual(base_config.base_bundle_id, config.base_bundle_id)
        self.assertEqual(base_config.provisioning_profile_basename,
                         config.provisioning_profile_basename)
        self.assertEqual('AppProduct-99.0.9999.99-Canary',
                         config.packaging_basename)

    def test_packaging_basename_channel(self):
        dist = model.Distribution(
            channel='dev',
            app_name_fragment='Dev',
            packaging_name_fragment='Dev',
            channel_customize=True)
        config = dist.to_config(TestConfig())
        self.assertEqual('App Product Dev', config.app_product)
        self.assertEqual('Product', config.product)
        self.assertEqual('test.signing.bundle_id.dev', config.base_bundle_id)
        self.assertEqual('provisiontest_Dev',
                         config.provisioning_profile_basename)
        self.assertEqual('AppProductDev-99.0.9999.99-Dev',
                         config.packaging_basename)


class TestPaths(unittest.TestCase):

    def test_construct(self):
        paths = model.Paths('[INPUT]', '[OUTPUT]', '[WORK]')
        self.assertEqual('[INPUT]', paths.input)
        self.assertEqual('[OUTPUT]', paths.output)
        self.assertEqual('[WORK]', paths.work)

    def test_packaging_dir(self):
        paths = model.Paths('[INPUT]', '[OUTPUT]', '[WORK]')
        packaging_dir = paths.packaging_dir(TestConfig())
        self.assertEqual('[INPUT]/Product Packaging', packaging_dir)

    def test_replace_work(self):
        paths = model.Paths('[INPUT]', '[OUTPUT]', '[WORK]')
        self.assertEqual('[WORK]', paths.work)
        paths2 = paths.replace_work('{WORK2}')
        self.assertEqual('[WORK]', paths.work)
        self.assertEqual('{WORK2}', paths2.work)
