# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import config


class TestConfig(config.CodeSignConfig):

    def __init__(self,
                 identity='[IDENTITY]',
                 installer_identity='[INSTALLER-IDENTITY]',
                 notary_user='[NOTARY-USER]',
                 notary_password='[NOTARY-PASSWORD]',
                 notary_asc_provider=None):
        super(TestConfig,
              self).__init__(identity, installer_identity, notary_user,
                             notary_password, notary_asc_provider)

    @staticmethod
    def is_chrome_branded():
        return True

    @property
    def app_product(self):
        return 'App Product'

    @property
    def product(self):
        return 'Product'

    @property
    def version(self):
        return '99.0.9999.99'

    @property
    def base_bundle_id(self):
        return 'test.signing.bundle_id'

    @property
    def provisioning_profile_basename(self):
        return 'provisiontest'

    @property
    def run_spctl_assess(self):
        return True


class TestConfigNonChromeBranded(TestConfig):

    @staticmethod
    def is_chrome_branded():
        return False
