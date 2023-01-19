# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import config


class TestConfig(config.CodeSignConfig):

    def __init__(self,
                 identity='[IDENTITY]',
                 installer_identity='[INSTALLER-IDENTITY]',
                 notary_user='[NOTARY-USER]',
                 notary_password='[NOTARY-PASSWORD]',
                 **kwargs):
        if 'notary_team_id' not in kwargs:
            kwargs['notary_team_id'] = '[NOTARY-TEAM]'
        super(TestConfig, self).__init__(identity, installer_identity,
                                         notary_user, notary_password, **kwargs)

    @staticmethod
    def is_chrome_branded():
        return True

    @staticmethod
    def enable_updater():
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

    @staticmethod
    def enable_updater():
        return False


class TestConfigInjectGetTaskAllow(TestConfig):

    @property
    def inject_get_task_allow_entitlement(self):
        return True
