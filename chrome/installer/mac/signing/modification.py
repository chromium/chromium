# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The modification module handles making changes to files in the application
bundle. At minimum, the Info.plists are modified to support Keystone and the
code signing entitlements file is added. In addition, other components might
be modified or renamed to support side-by-side channel installs.
"""

import os.path

from . import commands, signing

_CF_BUNDLE_EXE = 'CFBundleExecutable'
_CF_BUNDLE_ID = 'CFBundleIdentifier'
_ENT_APP_ID = 'com.apple.application-identifier'
_KS_BRAND_ID = 'KSBrandID'
_KS_CHANNEL_ID = 'KSChannelID'
_KS_PRODUCT_ID = 'KSProductID'


def _modify_plists(paths, dist, config):
    """Modifies several plist files in the bundle.

    This alters the CFBundleIdentifier if necessary and sets Keystone updater
    keys.

    Args:
        paths: A |model.Paths| object.
        dist: The |model.Distribution| for which customization is taking place.
        config: The |config.CodeSignConfig| object.
    """
    app_plist_path = os.path.join(paths.work, config.app_dir, 'Contents',
                                  'Info.plist')
    with commands.PlistContext(app_plist_path, rewrite=True) as app_plist:
        if dist.channel_customize:
            notification_xpc_plist_path = os.path.join(
                paths.work, config.framework_dir, 'XPCServices',
                'AlertNotificationService.xpc', 'Contents', 'Info.plist')
            with commands.PlistContext(
                    notification_xpc_plist_path,
                    rewrite=True) as notification_xpc_plist:
                notification_xpc_plist[_CF_BUNDLE_ID] = \
                        notification_xpc_plist[_CF_BUNDLE_ID].replace(
                                config.base_config.base_bundle_id,
                                config.base_bundle_id)

            app_plist[_CF_BUNDLE_ID] = config.base_bundle_id
            app_plist[_CF_BUNDLE_EXE] = config.app_product
            app_plist[_KS_PRODUCT_ID] += '.' + dist.channel

        # Apply the channel and brand code changes.
        if dist.branding_code:
            app_plist[_KS_BRAND_ID] = dist.branding_code
        elif _KS_BRAND_ID in app_plist:
            del app_plist[_KS_BRAND_ID]

        if dist.channel:
            app_plist[_KS_CHANNEL_ID] = dist.channel
        elif _KS_CHANNEL_ID in app_plist:
            del app_plist[_KS_CHANNEL_ID]

        if dist.product_dirname:
            app_plist['CrProductDirName'] = dist.product_dirname

        if dist.creator_code:
            app_plist['CFBundleSignature'] = dist.creator_code

        # See build/mac/tweak_info_plist.py and
        # chrome/browser/mac/keystone_glue.mm.
        for key in app_plist.keys():
            if not key.startswith(_KS_CHANNEL_ID + '-'):
                continue
            orig_channel, tag = key.split('-')
            channel_str = dist.channel if dist.channel else ''
            app_plist[key] = '{}-{}'.format(channel_str, tag)


def _replace_icons(paths, dist, config):
    """Replaces icon assets in the bundle with the channel-customized versions.

    Args:
        paths: A |model.Paths| object.
        dist: The |model.Distribution| for which customization is taking place.
        config: The |config.CodeSignConfig| object.
    """
    assert dist.channel_customize

    packaging_dir = paths.packaging_dir(config)
    resources_dir = os.path.join(paths.work, config.resources_dir)

    new_app_icon = os.path.join(packaging_dir,
                                'app_{}.icns'.format(dist.channel))
    new_document_icon = os.path.join(packaging_dir,
                                     'document_{}.icns'.format(dist.channel))

    commands.copy_files(new_app_icon, os.path.join(resources_dir, 'app.icns'))
    commands.copy_files(new_document_icon,
                        os.path.join(resources_dir, 'document.icns'))


def _rename_enterprise_manifest(paths, dist, config):
    """Modifies and renames the enterprise policy manifest files for channel-
    customized versions.

    Args:
        paths: A |model.Paths| object.
        dist: The |model.Distribution| for which customization is taking place.
        config: The |config.CodeSignConfig| object.
    """
    assert dist.channel_customize

    resources_dir = os.path.join(paths.work, config.resources_dir)

    old_name = '{}.manifest'.format(config.base_config.base_bundle_id)
    new_name = '{}.manifest'.format(config.base_bundle_id)

    commands.move_file(
        os.path.join(resources_dir, old_name, 'Contents', 'Resources',
                     old_name),
        os.path.join(resources_dir, old_name, 'Contents', 'Resources',
                     new_name))
    commands.move_file(
        os.path.join(resources_dir, old_name),
        os.path.join(resources_dir, new_name))

    manifest_plist_path = os.path.join(resources_dir, new_name, 'Contents',
                                       'Resources', new_name)
    with commands.PlistContext(
            manifest_plist_path, rewrite=True) as manifest_plist:
        manifest_plist['pfm_domain'] = config.base_bundle_id


def _process_entitlements(paths, dist, config):
    """Copies the entitlements file out of the Packaging directory to the work
    directory, possibly modifying it for channel customization.

    Args:
        paths: A |model.Paths| object.
        dist: The |model.Distribution| for which customization is taking place.
        config: The |config.CodeSignConfig| object.
    """
    packaging_dir = paths.packaging_dir(config)

    entitlements_names = [
        part.entitlements
        for part in signing.get_parts(config).values()
        if part.entitlements
    ]
    for entitlements_name in entitlements_names:
        entitlements_file = os.path.join(paths.work, entitlements_name)
        commands.copy_files(
            os.path.join(packaging_dir, entitlements_name), entitlements_file)

        if dist.channel_customize:
            with commands.PlistContext(
                    entitlements_file, rewrite=True) as entitlements:
                if _ENT_APP_ID in entitlements:
                    app_id = entitlements[_ENT_APP_ID]
                    entitlements[_ENT_APP_ID] = app_id.replace(
                        config.base_config.base_bundle_id,
                        config.base_bundle_id)


def customize_distribution(paths, dist, config):
    """Modifies the Chrome application bundle to specialize it with release
    channel information.

    Args:
        paths: A |model.Paths| object.
        dist: The |model.Distribution| for which customization is taking place.
        config: The |config.CodeSignConfig| object.
    """
    base_config = config.base_config

    # First, copy duplicate the bundle, optionally to a new name.
    if dist.channel_customize:
        commands.move_file(
            os.path.join(paths.work, base_config.app_dir),
            os.path.join(paths.work, config.app_dir))

        macos_dir = os.path.join(paths.work, config.app_dir, 'Contents',
                                 'MacOS')
        commands.move_file(
            os.path.join(macos_dir, base_config.app_product),
            os.path.join(macos_dir, config.app_product))

    _modify_plists(paths, dist, config)
    _process_entitlements(paths, dist, config)

    if dist.creator_code:
        pkg_info_file = os.path.join(paths.work, config.app_dir, 'Contents',
                                     'PkgInfo')
        commands.write_file(pkg_info_file, 'APPL{}'.format(dist.creator_code))

    if dist.channel_customize:
        _replace_icons(paths, dist, config)
        _rename_enterprise_manifest(paths, dist, config)
