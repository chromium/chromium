# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse

from signing import config, model, standard_invoker


class TestConfig(config.CodeSignConfig):

    def __init__(self, **kwargs):
        config_args = {
            'invoker': TestInvoker.factory_with_args(),
            'identity': '[IDENTITY]',
            'installer_identity': '[INSTALLER-IDENTITY]',
        }
        config_args.update(kwargs)
        super(TestConfig, self).__init__(**config_args)

    @staticmethod
    def is_chrome_branded():
        return True

    @property
    def enable_updater(self):
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

    @property
    def enable_updater(self):
        return False


class TestConfigInjectGetTaskAllow(TestConfig):

    @property
    def inject_get_task_allow_entitlement(self):
        return True


class TestConfigNotarizationToolOverride(TestConfig):

    @property
    def notarization_tool_path(self):
        return f'/fun/bin/{self.notarization_tool}.custom'


class TestInvoker(standard_invoker.Invoker):

    @staticmethod
    def factory_with_args(**kwargs):
        args = argparse.Namespace()
        args.notary_user = '[NOTARY-USER]'
        args.notary_password = '[NOTARY-PASSWORD]'
        args.notary_team_id = '[NOTARY-TEAM]'
        args.notary_asc_provider = None
        args.notarization_tool = model.NotarizationTool.ALTOOL
        args.__dict__.update(**kwargs)
        return lambda config: TestInvoker(args, config)
