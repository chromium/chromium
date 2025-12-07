# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The parts module defines the various binary pieces of the Updater application
bundle that need to be signed, as well as providing a function to sign them.
"""

import os.path

from signing import commands, signing
from signing.model import CodeSignOptions, CodeSignedProduct, VerifyOptions


def get_parts(config):
    """Returns all the |model.CodeSignedProduct| objects to be signed for an
    updater application bundle.

    Args:
        config: The |config.CodeSignConfig|.

    Returns:
        A list of |model.CodeSignedProduct|. Items should be signed in the
        order they appear in this list.
    """
    ks_bundle = (
        '{0.app_product}.app/Contents/Helpers/{0.keystone_app_name}.bundle'.
        format(config))
    ks_agent_app = (
        ks_bundle +
        '/Contents/Resources/{0.keystone_app_name}Agent.app'.format(config))

    # Innermost parts come first.
    return [
        CodeSignedProduct(  # Keystone Agent app bundle
            ks_agent_app,
            config.keystone_app_name + 'Agent',
            identifier_requirement=False,
            options=CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS,
            verify_options=VerifyOptions.DEEP | VerifyOptions.STRICT),
        CodeSignedProduct(  # Keystone's ksadmin
            ks_bundle + '/Contents/Helpers/ksadmin',
            'ksadmin',
            options=CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS,
            verify_options=VerifyOptions.DEEP | VerifyOptions.STRICT),
        CodeSignedProduct(  # Keystone's ksinstall
            ks_bundle + '/Contents/Helpers/ksinstall',
            'ksinstall',
            options=CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS,
            verify_options=VerifyOptions.DEEP | VerifyOptions.STRICT),
        CodeSignedProduct(  # Keystone bundle
            ks_bundle,
            config.keystone_app_name,
            options=CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS,
            verify_options=VerifyOptions.DEEP | VerifyOptions.STRICT),
        CodeSignedProduct(  # Updater Util
            '{.app_product}Util'.format(config),
            '{.app_product}Util'.format(config),
            options=CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS,
            verify_options=VerifyOptions.DEEP | VerifyOptions.STRICT),
        CodeSignedProduct(  # Updater bundle
            '{.app_product}.app/Contents/Helpers/launcher'.format(config),
            config.base_bundle_id,
            options=CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS,
            requirements=config.codesign_requirements_outer_app,
            identifier_requirement=False,
            entitlements=None,
            verify_options=VerifyOptions.DEEP | VerifyOptions.STRICT),
        CodeSignedProduct(  # Updater bundle
            '{.app_product}.app'.format(config),
            config.base_bundle_id,
            options=CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS,
            requirements=config.codesign_requirements_outer_app,
            identifier_requirement=False,
            entitlements=None,
            verify_options=VerifyOptions.DEEP | VerifyOptions.STRICT),
    ]


def sign_all(paths, config):
    """Code signs the application bundle and all of its internal nested code
    parts.

    Args:
        paths: A |model.Paths| object.
        config: The |model.CodeSignConfig| object. The |app_product| binary and
            nested binaries must exist in |paths.work|.
    """
    parts = get_parts(config)
    for part in parts:
        signing.sign_part(paths, config, part)
        signing.verify_part(paths, part)
    signing.validate_app(paths, config, parts[-1])
