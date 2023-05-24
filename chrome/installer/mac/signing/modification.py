# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The modification module handles making changes to files in the application
bundle. At minimum, the Info.plists are modified to support Keystone and the
code signing entitlements file is added. In addition, other components might
be modified or renamed to support side-by-side channel installs.
"""

import os.path

from signing import commands, parts

_CF_BUNDLE_DISPLAY_NAME = 'CFBundleDisplayName'
_CF_BUNDLE_EXE = 'CFBundleExecutable'
_CF_BUNDLE_ID = 'CFBundleIdentifier'
_CF_BUNDLE_NAME = 'CFBundleName'
_ENT_APP_ID = 'com.apple.application-identifier'
_ENT_GET_TASK_ALLOW = 'com.apple.security.get-task-allow'
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
            alert_helper_app_path = os.path.join(
                paths.work, config.framework_dir, 'Helpers',
                '{} Helper (Alerts).app'.format(config.product))
            alert_helper_plist_path = os.path.join(alert_helper_app_path,
                                                   'Contents', 'Info.plist')
            with commands.PlistContext(
                    alert_helper_plist_path,
                    rewrite=True) as alert_helper_plist:
                alert_helper_plist[_CF_BUNDLE_ID] = \
                        alert_helper_plist[_CF_BUNDLE_ID].replace(
                                config.base_config.base_bundle_id,
                                config.base_bundle_id)

            alert_helper_plist_strings_path = os.path.join(
                alert_helper_app_path, 'Contents', 'Resources', 'base.lproj',
                'InfoPlist.strings')
            with commands.PlistContext(
                    alert_helper_plist_strings_path, rewrite=True,
                    binary=True) as alert_helper_plist_strings:
                alert_helper_plist_strings[_CF_BUNDLE_DISPLAY_NAME] = \
                        '{} {}'.format(
                            alert_helper_plist_strings[_CF_BUNDLE_DISPLAY_NAME],
                            dist.app_name_fragment)

            app_plist[_CF_BUNDLE_DISPLAY_NAME] = '{} {}'.format(
                app_plist[_CF_BUNDLE_DISPLAY_NAME], dist.app_name_fragment)
            app_plist[_CF_BUNDLE_EXE] = config.app_product
            app_plist[_CF_BUNDLE_ID] = config.base_bundle_id
            app_plist[_CF_BUNDLE_NAME] = '{} {}'.format(
                app_plist[_CF_BUNDLE_NAME], dist.app_name_fragment)
            app_plist[_KS_PRODUCT_ID] += '.' + dist.channel

        # Apply the channel and brand code changes.
        if dist.branding_code:
            app_plist[_KS_BRAND_ID] = dist.branding_code
        elif _KS_BRAND_ID in app_plist:
            del app_plist[_KS_BRAND_ID]

        base_tag = app_plist.get(_KS_CHANNEL_ID)
        base_channel_tag_components = []
        if base_tag:
            base_channel_tag_components.append(base_tag)
        if dist.channel:
            base_channel_tag_components.append(dist.channel)
        base_channel_tag = '-'.join(base_channel_tag_components)
        if base_channel_tag:
            app_plist[_KS_CHANNEL_ID] = base_channel_tag
        elif _KS_CHANNEL_ID in app_plist:
            del app_plist[_KS_CHANNEL_ID]

        if dist.product_dirname:
            app_plist['CrProductDirName'] = dist.product_dirname

        if dist.creator_code:
            app_plist['CFBundleSignature'] = dist.creator_code

        # See build/apple/tweak_info_plist.py and
        # chrome/browser/mac/keystone_glue.mm.
        for key in app_plist.keys():
            if not key.startswith(_KS_CHANNEL_ID + '-'):
                continue
            ignore, extra = key.split('-')
            app_plist[key] = '{}-{}'.format(base_channel_tag, extra)


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

    # Also update the icon in the Alert Helper app.
    alert_helper_resources_dir = os.path.join(
        paths.work, config.framework_dir, 'Helpers',
        '{} Helper (Alerts).app'.format(config.product), 'Contents',
        'Resources')
    commands.copy_files(new_app_icon,
                        os.path.join(alert_helper_resources_dir, 'app.icns'))


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
        for part in parts.get_parts(config).values()
        if part.entitlements
    ]
    for entitlements_name in sorted(entitlements_names):
        entitlements_file = os.path.join(paths.work, entitlements_name)
        commands.copy_files(
            os.path.join(packaging_dir, entitlements_name), entitlements_file)

        if dist.channel_customize or config.inject_get_task_allow_entitlement:
            with commands.PlistContext(
                    entitlements_file, rewrite=True) as entitlements:
                if dist.channel_customize and _ENT_APP_ID in entitlements:
                    app_id = entitlements[_ENT_APP_ID]
                    entitlements[_ENT_APP_ID] = app_id.replace(
                        config.base_config.base_bundle_id,
                        config.base_bundle_id)
                if config.inject_get_task_allow_entitlement:
                    entitlements[_ENT_GET_TASK_ALLOW] = True


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
