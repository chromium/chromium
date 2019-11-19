# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The pipeline module orchestrates the entire signing process, which includes:
    1. Customizing build products for release channels.
    2. Code signing the application bundle and all of its nested code.
    3. Producing a packaged DMG.
    4. Signing and packaging the installer tools.
"""

import os.path

from . import commands, model, modification, notarize, signing


def _customize_and_sign_chrome(paths, dist_config, dest_dir, signed_frameworks):
    """Does channel customization and signing of a Chrome distribution. The
    resulting app bundle is moved into |dest_dir|.

    Args:
        paths: A |model.Paths| object.
        dist_config: A |config.CodeSignConfig| for the |model.Distribution|.
        dest_dir: The directory into which the product will be placed when
            the operations are completed.
        signed_frameworks: A dict that will store paths and change counts of
            already-signed inner frameworks keyed by bundle ID. Paths are used
            to recycle already-signed frameworks instead of re-signing them.
            Change counts are used to verify equivalence of frameworks when
            recycling them. Callers can pass an empty dict on the first call,
            and reuse the same dict for subsequent calls. This function will
            produce and consume entries in the dict. If this sharing is
            undesired, pass None instead of a dict.
    """
    # Copy the app to sign into the work dir.
    commands.copy_files(
        os.path.join(paths.input, dist_config.base_config.app_dir), paths.work)

    # Customize the app bundle.
    modification.customize_distribution(paths, dist_config.distribution,
                                        dist_config)

    work_dir_framework_path = os.path.join(paths.work,
                                           dist_config.framework_dir)
    if signed_frameworks is not None and dist_config.base_bundle_id in signed_frameworks:
        # If the inner framework has already been modified and signed for this
        # bundle ID, recycle the existing signed copy without signing a new
        # copy. This ensures that bit-for-bit identical input will result in
        # bit-for-bit identical signatures not affected by differences in, for
        # example, the signature's timestamp. All variants of a product sharing
        # the same bundle ID are assumed to have bit-for-bit identical
        # frameworks.
        #
        # This is significant because of how binary diff updates work. Binary
        # diffs are built between two successive versions on the basis of their
        # inner frameworks being bit-for-bit identical without regard to any
        # customizations applied only to the outer app. In order for these to
        # apply to all installations regardless of the presence or specific
        # values of any app-level customizations, all inner frameworks for a
        # single version and base bundle ID must always remain bit-for-bit
        # identical, including their signatures.
        (signed_framework_path, signed_framework_change_count
        ) = signed_frameworks[dist_config.base_bundle_id]
        actual_framework_change_count = commands.copy_dir_overwrite_and_count_changes(
            os.path.join(dest_dir, signed_framework_path),
            work_dir_framework_path,
            dry_run=False)

        if actual_framework_change_count != signed_framework_change_count:
            raise ValueError(
                'While customizing and signing {} ({}), actual_framework_change_count {} != signed_framework_change_count {}'
                .format(dist_config.base_bundle_id,
                        dist_config.packaging_basename,
                        actual_framework_change_count,
                        signed_framework_change_count))

        signing.sign_chrome(paths, dist_config, sign_framework=False)
    else:
        unsigned_framework_path = os.path.join(paths.work,
                                               'modified_unsigned_framework')
        commands.copy_dir_overwrite_and_count_changes(
            work_dir_framework_path, unsigned_framework_path, dry_run=False)
        signing.sign_chrome(paths, dist_config, sign_framework=True)
        actual_framework_change_count = commands.copy_dir_overwrite_and_count_changes(
            work_dir_framework_path, unsigned_framework_path, dry_run=True)
        if signed_frameworks is not None:
            dest_dir_framework_path = os.path.join(dest_dir,
                                                   dist_config.framework_dir)
            signed_frameworks[dist_config.base_bundle_id] = (
                dest_dir_framework_path, actual_framework_change_count)

    app_path = os.path.join(paths.work, dist_config.app_dir)
    commands.make_dir(dest_dir)
    commands.move_file(app_path, os.path.join(dest_dir, dist_config.app_dir))


def _staple_chrome(paths, dist_config):
    """Staples all the executable components of the Chrome app bundle.

    Args:
        paths: A |model.Paths| object.
        dist_config: A |config.CodeSignConfig| for the customized product.
    """
    parts = signing.get_parts(dist_config)
    # Only staple the signed, bundled executables.
    part_paths = [
        part.path
        for part in parts.values()
        # TODO(https://crbug.com/979725): Reinstate .xpc bundle stapling once
        # the signing environment is on a macOS release that supports
        # Xcode 10.2 or newer.
        if part.path[-4:] in ('.app',)
    ]
    # Reverse-sort the paths so that more nested paths are stapled before
    # less-nested ones.
    part_paths.sort(reverse=True)
    for part_path in part_paths:
        notarize.staple(os.path.join(paths.work, part_path))


def _productbuild_distribution_path(paths, dist_config, component_pkg_path):
    """Creates a distribution XML file for use by `productbuild`. This specifies
    that an x64 machine is required, and copies the OS requirement from the copy
    of Chrome being packaged.

    Args:
        paths: A |model.Paths| object.
        dist_config: The |config.CodeSignConfig| object.
        component_pkg_path: The path to the existing component .pkg file.

    Returns:
        The path to the distribution file.
    """
    distribution_path = os.path.join(paths.work,
                                     '{}.dist'.format(dist_config.app_product))

    app_plist_path = os.path.join(paths.work, dist_config.app_dir, 'Contents',
                                  'Info.plist')
    with commands.PlistContext(app_plist_path) as app_plist:
        distribution_xml = """<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">

    <!-- Top-level info about the distribution. -->
    <title>{app_product}</title>
    <options customize="never" require-scripts="false" hostArchitectures="x86_64"/>
    <volume-check>
        <allowed-os-versions>
            <os-version min="{minimum_system}"/>
        </allowed-os-versions>
    </volume-check>

    <!-- The hierarchy of installation choices. -->
    <choices-outline>
        <line choice="default">
            <line choice="{bundle_id}"/>
        </line>
    </choices-outline>

    <!-- The individual choices. -->
    <choice id="default"/>
    <choice id="{bundle_id}" visible="false" title="{app_product}">
        <pkg-ref id="{bundle_id}"/>
    </choice>

    <!-- The lone component package. -->
    <pkg-ref id="{bundle_id}" version="{version}" onConclusion="none">{component_pkg_filename}</pkg-ref>

</installer-gui-script>""".format(
            app_product=dist_config.app_product,
            bundle_id=dist_config.base_bundle_id,
            minimum_system=app_plist['LSMinimumSystemVersion'],
            component_pkg_filename=os.path.basename(component_pkg_path),
            version=dist_config.version)

        commands.write_file(distribution_path, distribution_xml)

    return distribution_path


def _package_and_sign_pkg(paths, dist_config):
    """Packages, signs, and verifies a PKG for a signed build product.

    Args:
        paths: A |model.Paths| object.
        dist_config: The |config.CodeSignConfig| object.

    Returns:
        The path to the signed PKG file.
    """
    assert dist_config.installer_identity

    # There are two .pkg files to be built:
    #   1. The inner component package (which is the one that can contain things
    #      like postinstall scripts). This is built with `pkgbuild`.
    #   2. The outer product archive (which is the installable thing that has
    #      pre-install requirements). This is built with `productbuild`.

    ## The component package.

    # The spaces are removed from |dist_config.app_product| for the component
    # package path due to a bug in Installer.app that causes the "Show Files"
    # window to be blank if there is a space in a component package name.
    # https://stackoverflow.com/questions/43031272/
    component_pkg_name = '{}.pkg'.format(dist_config.app_product).replace(
        ' ', '')
    component_pkg_path = os.path.join(paths.work, component_pkg_name)
    app_path = os.path.join(paths.work, dist_config.app_dir)

    commands.run_command([
        'pkgbuild', '--identifier', dist_config.base_bundle_id, '--version',
        dist_config.version, '--component', app_path, '--install-location',
        '/Applications', component_pkg_path
    ])

    ## The product archive.

    distribution_path = _productbuild_distribution_path(paths, dist_config,
                                                        component_pkg_path)

    product_pkg_path = os.path.join(
        paths.output, '{}.pkg'.format(dist_config.packaging_basename))

    command = [
        'productbuild', '--distribution', distribution_path, '--package-path',
        paths.work, '--sign', dist_config.installer_identity
    ]
    if dist_config.notary_user:
        # Assume if the config has notary authentication information that the
        # products will be notarized, which requires a secure timestamp.
        command.append('--timestamp')
    command.append(product_pkg_path)
    commands.run_command(command)

    return product_pkg_path


def _package_and_sign_dmg(paths, dist_config):
    """Packages, signs, and verifies a DMG for a signed build product.

    Args:
        paths: A |model.Paths| object.
        dist_config: The |config.CodeSignConfig| object.

    Returns:
        The path to the signed DMG file.
    """
    dist = dist_config.distribution

    dmg_path = _package_dmg(paths, dist, dist_config)

    # dmg_identifier is like dmg_name but without the file extension. If a brand
    # code is in use, use the actual brand code instead of the name fragment, to
    # avoid leaking the association between brand codes and their meanings.
    dmg_identifier = dist_config.packaging_basename
    if dist.branding_code:
        dmg_identifier = dist_config.packaging_basename.replace(
            dist.packaging_name_fragment, dist.branding_code)

    product = model.CodeSignedProduct(
        dmg_path, dmg_identifier, sign_with_identifier=True)
    signing.sign_part(paths, dist_config, product)
    signing.verify_part(paths, product)

    return dmg_path


def _package_dmg(paths, dist, config):
    """Packages a Chrome application bundle into a DMG.

    Args:
        paths: A |model.Paths| object.
        dist: The |model.Distribution| for which the product was customized.
        config: The |config.CodeSignConfig| object.

    Returns:
        A path to the produced DMG file.
    """
    packaging_dir = paths.packaging_dir(config)

    if dist.channel_customize:
        dsstore_file = 'chrome_{}_dmg_dsstore'.format(dist.channel)
        icon_file = 'chrome_{}_dmg_icon.icns'.format(dist.channel)
    else:
        dsstore_file = 'chrome_dmg_dsstore'
        icon_file = 'chrome_dmg_icon.icns'

    dmg_path = os.path.join(paths.output,
                            '{}.dmg'.format(config.packaging_basename))
    app_path = os.path.join(paths.work, config.app_dir)

    # A locally-created empty directory is more trustworthy than /var/empty.
    empty_dir = os.path.join(paths.work, 'empty')
    commands.make_dir(empty_dir)

    # Make the disk image. Don't include any customized name fragments in
    # --volname because the .DS_Store expects the volume name to be constant.
    # Don't put a name on the /Applications symbolic link because the same disk
    # image is used for all languages.
    # yapf: disable
    commands.run_command([
        os.path.join(packaging_dir, 'pkg-dmg'),
        '--verbosity', '0',
        '--tempdir', paths.work,
        '--source', empty_dir,
        '--target', dmg_path,
        '--format', 'UDBZ',
        '--volname', config.app_product,
        '--icon', os.path.join(packaging_dir, icon_file),
        '--copy', '{}:/'.format(app_path),
        '--copy',
            '{}/keystone_install.sh:/.keystone_install'.format(packaging_dir),
        '--mkdir', '.background',
        '--copy',
            '{}/chrome_dmg_background.png:/.background/background.png'.format(
                packaging_dir),
        '--copy', '{}/{}:/.DS_Store'.format(packaging_dir, dsstore_file),
        '--symlink', '/Applications:/ ',
    ])
    # yapf: enable

    return dmg_path


def _package_installer_tools(paths, config):
    """Signs and packages all the installer tools, which are not shipped to end-
    users.

    Args:
        paths: A |model.Paths| object.
        config: The |config.CodeSignConfig| object.
    """
    DIFF_TOOLS = 'diff_tools'

    tools_to_sign = signing.get_installer_tools(config)
    other_tools = (
        'dirdiffer.sh',
        'dirpatcher.sh',
        'dmgdiffer.sh',
        'keystone_install.sh',
        'pkg-dmg',
    )

    with commands.WorkDirectory(paths) as paths:
        diff_tools_dir = os.path.join(paths.work, DIFF_TOOLS)
        commands.make_dir(diff_tools_dir)

        for part in tools_to_sign.values():
            commands.copy_files(
                os.path.join(paths.input, part.path), diff_tools_dir)
            part.path = os.path.join(DIFF_TOOLS, os.path.basename(part.path))
            signing.sign_part(paths, config, part)

        for part in tools_to_sign.values():
            signing.verify_part(paths, part)

        for tool in other_tools:
            commands.copy_files(
                os.path.join(paths.packaging_dir(config), tool), diff_tools_dir)

        zip_file = os.path.join(paths.output, DIFF_TOOLS + '.zip')
        commands.run_command(['zip', '-9ry', zip_file, DIFF_TOOLS],
                             cwd=paths.work)


def _intermediate_work_dir_name(dist_config):
    """Returns the name of an intermediate work directory for a distribution.

    Args:
        dist_config: A |config.CodeSignConfig| for the |model.Distribution|.

    Returns:
        The work directory name to use.
    """
    if dist_config.distribution.branding_code:
        return '{}-{}'.format(dist_config.packaging_basename,
                              dist_config.distribution.branding_code)

    return dist_config.packaging_basename


def sign_all(orig_paths,
             config,
             disable_packaging=False,
             do_notarization=True,
             skip_brands=[]):
    """For each distribution in |config|, performs customization, signing, and
    DMG packaging and places the resulting signed DMG in |orig_paths.output|.
    The |paths.input| must contain the products to customize and sign.

    Args:
        orig_paths: A |model.Paths| object.
        config: The |config.CodeSignConfig| object.
        package_dmg: If True, the signed application bundle will be packaged
            into a DMG, which will also be signed. If False, the signed app
            bundle will be copied to |paths.output|.
        do_notarization: If True, the signed application bundle will be sent for
            notarization by Apple. The resulting notarization ticket will then
            be stapled. If |package_dmg| is also True, the stapled application
            will be packaged in the DMG and then the DMG itself will be
            notarized and stapled.
        skip_brands: A list of brand code strings. If a distribution has a brand
            code in this list, that distribution will be skipped.
    """
    with commands.WorkDirectory(orig_paths) as notary_paths:
        # First, sign all the distributions and optionally submit the
        # notarization requests.
        uuids_to_config = {}
        signed_frameworks = {}
        for dist in config.distributions:
            if dist.branding_code in skip_brands:
                continue

            with commands.WorkDirectory(orig_paths) as paths:
                dist_config = dist.to_config(config)
                do_packaging = (dist.package_as_dmg or
                                dist.package_as_pkg) and not disable_packaging

                # If not packaging and not notarizing, then simply drop the
                # signed bundle in the output directory when done signing.
                if not do_packaging and not do_notarization:
                    dest_dir = paths.output
                else:
                    dest_dir = notary_paths.work

                dest_dir = os.path.join(
                    dest_dir, _intermediate_work_dir_name(dist_config))
                _customize_and_sign_chrome(paths, dist_config, dest_dir,
                                           signed_frameworks)

                # If the build products are to be notarized, ZIP the app bundle
                # and submit it for notarization.
                if do_notarization:
                    zip_file = os.path.join(
                        notary_paths.work,
                        dist_config.packaging_basename + '.zip')
                    commands.run_command([
                        'zip', '--recurse-paths', '--symlinks', '--quiet',
                        zip_file, dist_config.app_dir
                    ],
                                         cwd=dest_dir)
                    uuid = notarize.submit(zip_file, dist_config)
                    uuids_to_config[uuid] = dist_config

        # Wait for app notarization results to come back, stapling as they do.
        if do_notarization:
            for result in notarize.wait_for_results(uuids_to_config.keys(),
                                                    config):
                dist_config = uuids_to_config[result]
                dest_dir = os.path.join(
                    notary_paths.work, _intermediate_work_dir_name(dist_config))
                _staple_chrome(notary_paths.replace_work(dest_dir), dist_config)

        # After all apps are optionally notarized, package as required.
        if not disable_packaging:
            uuids_to_package_path = {}
            for dist in config.distributions:
                if dist.branding_code in skip_brands:
                    continue

                dist_config = dist.to_config(config)
                paths = orig_paths.replace_work(
                    os.path.join(notary_paths.work,
                                 _intermediate_work_dir_name(dist_config)))

                if dist.package_as_dmg:
                    dmg_path = _package_and_sign_dmg(paths, dist_config)

                    if do_notarization:
                        uuid = notarize.submit(dmg_path, dist_config)
                        uuids_to_package_path[uuid] = dmg_path

                if dist.package_as_pkg:
                    pkg_path = _package_and_sign_pkg(paths, dist_config)

                    if do_notarization:
                        uuid = notarize.submit(pkg_path, dist_config)
                        uuids_to_package_path[uuid] = pkg_path

            # Wait for packaging notarization results to come back, stapling as
            # they do.
            if do_notarization:
                for result in notarize.wait_for_results(
                        uuids_to_package_path.keys(), config):
                    package_path = uuids_to_package_path[result]
                    notarize.staple(package_path)

    _package_installer_tools(orig_paths, config)
