# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The parts module defines the various binary pieces of the Chrome application
bundle that need to be signed, as well as providing utilities to sign them.
"""

import os.path

from signing import commands, signing
from signing.model import CodeSignOptions, CodeSignedProduct, VerifyOptions

_PROVISIONPROFILE_EXT = '.provisionprofile'
_PROVISIONPROFILE_DEST = 'embedded.provisionprofile'


def get_parts(config):
    """Returns all the |model.CodeSignedProduct| objects to be signed for a
    Chrome application bundle.

    Args:
        config: The |config.CodeSignConfig|.

    Returns:
        A dictionary of |model.CodeSignedProduct|. The keys are short
        identifiers that have no bearing on the actual signing operations.
    """
    # Inner parts of the bundle do not have the identifier customized with
    # the channel's identifier fragment.
    if hasattr(config, 'base_config'):
        uncustomized_bundle_id = config.base_config.base_bundle_id
    else:
        uncustomized_bundle_id = config.base_bundle_id

    verify_options = VerifyOptions.DEEP | VerifyOptions.STRICT

    parts = {
        'app':
            CodeSignedProduct(
                '{.app_product}.app'.format(config),
                config.base_bundle_id,
                options=CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS,
                requirements=config.codesign_requirements_outer_app,
                identifier_requirement=False,
                entitlements='app-entitlements.plist',
                verify_options=verify_options),
        'framework':
            CodeSignedProduct(
                # The framework is a dylib, so options= flags are meaningless.
                config.framework_dir,
                '{}.framework'.format(uncustomized_bundle_id),
                verify_options=verify_options),
        'crashpad':
            CodeSignedProduct(
                '{.framework_dir}/Helpers/chrome_crashpad_handler'.format(
                    config),
                'chrome_crashpad_handler',
                options=CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS,
                verify_options=verify_options),
        'helper-app':
            CodeSignedProduct(
                '{0.framework_dir}/Helpers/{0.product} Helper.app'.format(
                    config),
                '{}.helper'.format(uncustomized_bundle_id),
                options=CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS,
                verify_options=verify_options),
        'helper-renderer-app':
            CodeSignedProduct(
                '{0.framework_dir}/Helpers/{0.product} Helper (Renderer).app'
                .format(config),
                '{}.helper.renderer'.format(uncustomized_bundle_id),
                # Do not use |CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS|
                # because library validation is incompatible with the JIT
                # entitlement.
                options=CodeSignOptions.RESTRICT | CodeSignOptions.KILL
                | CodeSignOptions.HARDENED_RUNTIME,
                entitlements='helper-renderer-entitlements.plist',
                verify_options=verify_options),
        'helper-gpu-app':
            CodeSignedProduct(
                '{0.framework_dir}/Helpers/{0.product} Helper (GPU).app'.format(
                    config),
                '{}.helper'.format(uncustomized_bundle_id),
                # Do not use |CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS|
                # because library validation is incompatible with more
                # permissive code signing entitlements.
                options=CodeSignOptions.RESTRICT | CodeSignOptions.KILL
                | CodeSignOptions.HARDENED_RUNTIME,
                entitlements='helper-gpu-entitlements.plist',
                verify_options=verify_options),
        'helper-plugin-app':
            CodeSignedProduct(
                '{0.framework_dir}/Helpers/{0.product} Helper (Plugin).app'
                .format(config),
                '{}.helper.plugin'.format(uncustomized_bundle_id),
                # Do not use |CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS|
                # because library validation is incompatible with the
                # disable-library-validation entitlement.
                options=CodeSignOptions.RESTRICT | CodeSignOptions.KILL
                | CodeSignOptions.HARDENED_RUNTIME,
                entitlements='helper-plugin-entitlements.plist',
                verify_options=verify_options),
        'helper-alerts':
            CodeSignedProduct(
                '{0.framework_dir}/Helpers/{0.product} Helper (Alerts).app'
                .format(config),
                '{}.framework.AlertNotificationService'.format(
                    config.base_bundle_id),
                options=CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS,
                verify_options=verify_options),
        'app-mode-app':
            CodeSignedProduct(
                '{.framework_dir}/Helpers/app_mode_loader'.format(config),
                'app_mode_loader',
                options=CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS,
                verify_options=verify_options),
        'web-app-shortcut-copier':
            CodeSignedProduct(
                '{.framework_dir}/Helpers/web_app_shortcut_copier'.format(
                    config),
                '{}.web_app_shortcut_copier'.format(uncustomized_bundle_id),
                options=CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS,
                sign_with_identifier=True,
                verify_options=verify_options),
    }

    if config.enable_updater:
        parts['privileged-helper'] = CodeSignedProduct(
            ('{.app_product}.app/Contents/Library/LaunchServices/' +
             '{}.UpdaterPrivilegedHelper').format(config,
                                                  uncustomized_bundle_id),
            '{}.UpdaterPrivilegedHelper'.format(uncustomized_bundle_id),
            options=CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS,
            verify_options=verify_options)

    dylibs = [
        'libEGL.dylib',
        'libGLESv2.dylib',
        'libvk_swiftshader.dylib',
    ]
    if config.is_chrome_branded():
        dylibs.append('liboptimization_guide_internal.dylib')
    for library in dylibs:
        library_basename = os.path.basename(library)
        parts[library_basename] = CodeSignedProduct(
            '{.framework_dir}/Libraries/{library}'.format(
                config, library=library),
            library_basename.replace('.dylib', ''),
            verify_options=verify_options)

    return parts


def get_installer_tools(config):
    """Returns all the |model.CodeSignedProduct| objects to be signed for
    creating the installer tools package.

    Args:
        config: The |config.CodeSignConfig|.

    Returns:
        A dictionary of |model.CodeSignedProduct|. The keys are short
        identifiers that have no bearing on the actual signing operations.
    """
    tools = {}
    binaries = (
        'goobsdiff',
        'goobspatch',
        'liblzma_decompress.dylib',
        'xz',
        'xzdec',
    )
    for binary in binaries:
        options = (
            CodeSignOptions.HARDENED_RUNTIME | CodeSignOptions.RESTRICT
            | CodeSignOptions.LIBRARY_VALIDATION | CodeSignOptions.KILL)
        tools[binary] = CodeSignedProduct(
            '{.packaging_dir}/{binary}'.format(config, binary=binary),
            binary.replace('.dylib', ''),
            options=options if not binary.endswith('dylib') else None,
            verify_options=VerifyOptions.DEEP | VerifyOptions.STRICT)

    return tools


def sign_chrome(paths, config, sign_framework=False):
    """Code signs the Chrome application bundle and all of its internal nested
    code parts.

    Args:
        paths: A |model.Paths| object.
        config: The |model.CodeSignConfig| object. The |app_product| binary and
            nested binaries must exist in |paths.work|.
        sign_framework: True if the inner framework is to be signed in addition
            to the outer application. False if only the outer application is to
            be signed.
    """
    parts = get_parts(config)

    _sanity_check_version_keys(paths, parts)

    if sign_framework:
        # To sign an .app bundle that contains nested code, the nested
        # components themselves must be signed. Each of these components is
        # signed below. Note that unless a framework has multiple versions
        # (which is discouraged), signing the entire framework is equivalent to
        # signing the Current version.
        # https://developer.apple.com/library/content/technotes/tn2206/_index.html#//apple_ref/doc/uid/DTS40007919-CH1-TNTAG13
        for name, part in parts.items():
            if name in ('app', 'framework', 'privileged-helper'):
                continue
            signing.sign_part(paths, config, part)

        # Sign the framework bundle.
        signing.sign_part(paths, config, parts['framework'])

    provisioning_profile_basename = config.provisioning_profile_basename
    if provisioning_profile_basename:
        commands.copy_files(
            os.path.join(
                paths.packaging_dir(config),
                provisioning_profile_basename + _PROVISIONPROFILE_EXT),
            os.path.join(paths.work, parts['app'].path, 'Contents',
                         _PROVISIONPROFILE_DEST))

    # Sign the privileged helper.
    if 'privileged-helper' in parts:
        signing.sign_part(paths, config, parts['privileged-helper'])

    # Sign the outer app bundle.
    signing.sign_part(paths, config, parts['app'])

    # Verify all the parts.
    for part in parts.values():
        signing.verify_part(paths, part)

    # Display the code signature.
    signing.validate_app(paths, config, parts['app'])

    # Validate the app's architecture geometry, if configured.
    signing.validate_app_geometry(paths, config, parts['app'])


def _sanity_check_version_keys(paths, parts):
    """Verifies that the various version keys in Info.plists match.

    Args:
        paths: A |model.Paths| object.
        parts: The dictionary returned from get_parts().
    """
    app_plist_path = os.path.join(paths.work, parts['app'].path, 'Contents',
                                  'Info.plist')
    framework_plist_path = os.path.join(paths.work, parts['framework'].path,
                                        'Resources', 'Info.plist')

    with commands.PlistContext(
            app_plist_path) as app_plist, commands.PlistContext(
                framework_plist_path) as framework_plist:
        if not 'KSVersion' in app_plist:
            assert 'com.google.Chrome' not in app_plist['CFBundleIdentifier']
            return
        ks_version = app_plist['KSVersion']
        cf_version = framework_plist['CFBundleShortVersionString']
        if cf_version != ks_version:
            raise ValueError(
                'CFBundleVersion ({}) does not mach KSVersion ({})'.format(
                    cf_version, ks_version))
