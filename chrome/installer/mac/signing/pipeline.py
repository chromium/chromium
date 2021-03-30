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
import plistlib

from . import commands, model, modification, notarize, parts, signing


def _include_branding_code_in_app(dist):
    """Returns whether to omit the branding code from the Chrome .app bundle.

    If a distribution is packaged in a PKG (but is not also packaged in a DMG),
    then the brand code is carried in the PKG script, and should not be added to
    the .app bundle's Info.plist.

    Args:
        dist: The |model.Distribution|.

    Returns:
        Whether to include the branding code in the app bundle.
    """
    return dist.package_as_dmg or not dist.package_as_pkg


def _binary_architectures(binary_path):
    """Returns a comma-separated list of architectures of a binary.

    Args:
        binary_path: The path to the binary on disk.

    Returns:
        A comma-separated string of architectures.
    """

    command = ['lipo', '-archs', binary_path]
    output = commands.run_command_output(command)
    output = output.strip()
    output = output.replace(' ', ',')

    return output


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
    customization_dist = dist_config.distribution
    customization_dist_config = dist_config
    if not _include_branding_code_in_app(customization_dist):
        customization_dist = dist_config.distribution.brandless_copy()
        customization_dist_config = customization_dist.to_config(
            dist_config.base_config)

    modification.customize_distribution(paths, customization_dist,
                                        customization_dist_config)

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

        parts.sign_chrome(paths, dist_config, sign_framework=False)
    else:
        unsigned_framework_path = os.path.join(paths.work,
                                               'modified_unsigned_framework')
        commands.copy_dir_overwrite_and_count_changes(
            work_dir_framework_path, unsigned_framework_path, dry_run=False)
        parts.sign_chrome(paths, dist_config, sign_framework=True)
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
    notarize.staple_bundled_parts(parts.get_parts(dist_config).values(), paths)


def _create_pkgbuild_scripts(paths, dist_config):
    """Creates a directory filled with scripts for use by `pkgbuild`, and copies
    the postinstall script into this directory, customizing it along the way.

    Args:
        paths: A |model.Paths| object.
        dist_config: The |config.CodeSignConfig| object.

    Returns:
        The path to the scripts directory.
    """
    scripts_path = os.path.join(paths.work, 'scripts')
    commands.make_dir(scripts_path)

    packaging_dir = paths.packaging_dir(dist_config)

    def do_substitutions(script):
        substitutions = {
            '@APP_DIR@': dist_config.app_dir,
            '@APP_PRODUCT@': dist_config.app_product,
            '@BRAND_CODE@': dist_config.distribution.branding_code or '',
            '@FRAMEWORK_DIR@': dist_config.framework_dir
        }
        for key, value in substitutions.items():
            script = script.replace(key, value)

        return script

    postinstall_src_path = os.path.join(packaging_dir, 'pkg_postinstall.in')
    postinstall_dest_path = os.path.join(scripts_path, 'postinstall')

    postinstall = commands.read_file(postinstall_src_path)
    postinstall = do_substitutions(postinstall)
    commands.write_file(postinstall_dest_path, postinstall)
    commands.set_executable(postinstall_dest_path)

    return scripts_path


def _component_property_path(paths, dist_config):
    """Creates a component plist file for use by `pkgbuild`. The reason this
    file is used is to ensure that the component package is not relocatable. See
    https://scriptingosx.com/2017/05/relocatable-package-installers-and-quickpkg-update/
    for information on why that's important.

    Args:
        paths: A |model.Paths| object.
        dist_config: The |config.CodeSignConfig| object.

    Returns:
        The path to the component plist file.
    """
    component_property_path = os.path.join(
        paths.work, '{}.plist'.format(dist_config.app_product))

    plistlib.writePlist([{
        'BundleHasStrictIdentifier': True,
        'BundleIsRelocatable': False,
        'BundleIsVersionChecked': True,
        'BundleOverwriteAction': 'upgrade',
        'RootRelativeBundlePath': dist_config.app_dir
    }], component_property_path)

    return component_property_path


def _productbuild_distribution_path(app_paths, pkg_paths, dist_config,
                                    component_pkg_path):
    """Creates a distribution XML file for use by `productbuild`. This copies
    the OS and architecture requirements from the copy of Chrome being packaged.

    Args:
        app_paths: A |model.Paths| object for the app.
        pkg_paths: A |model.Paths| object for the pkg files.
        dist_config: The |config.CodeSignConfig| object.
        component_pkg_path: The path to the existing component .pkg file.

    Returns:
        The path to the distribution file.
    """
    distribution_path = os.path.join(pkg_paths.work,
                                     '{}.dist'.format(dist_config.app_product))

    app_binary_path = os.path.join(app_paths.work, dist_config.app_dir,
                                   'Contents', 'MacOS', dist_config.app_product)
    app_plist_path = os.path.join(app_paths.work, dist_config.app_dir,
                                  'Contents', 'Info.plist')
    with commands.PlistContext(app_plist_path) as app_plist:
        # For now, restrict installation to only the boot volume (the <domains/>
        # tag) to simplify the Keystone installation.
        distribution_xml = """<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">

    <!-- Top-level info about the distribution. -->
    <title>{app_product}</title>
    <options customize="never" require-scripts="false" hostArchitectures="{host_architectures}"/>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
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
        <pkg-ref id="{bundle_id}">
            <must-close>
                <app id="{bundle_id}"/>
            </must-close>
        </pkg-ref>
    </choice>

    <!-- The lone component package. -->
    <pkg-ref id="{bundle_id}" version="{version}" onConclusion="none">{component_pkg_filename}</pkg-ref>

</installer-gui-script>""".format(
            app_product=dist_config.app_product,
            bundle_id=dist_config.base_bundle_id,
            host_architectures=_binary_architectures(app_binary_path),
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

    # Because several .pkg distributions might be built from the same underlying
    # .app, separate the .pkg construction into its own work directory.
    with commands.WorkDirectory(paths) as pkg_paths:
        # There are two .pkg files to be built:
        #   1. The inner component package (which is the one that can contain
        #      things like postinstall scripts). This is built with `pkgbuild`.
        #   2. The outer distribution package (which is the installable thing
        #      that has pre-install requirements). This is built with
        #      `productbuild`.

        ## The component package.

        # Because the component package is built using the --root option, copy
        # the .app into a directory by itself, as `pkgbuild` archives the entire
        # directory specified as the root directory.
        root_directory = os.path.join(pkg_paths.work, 'payload')
        commands.make_dir(root_directory)
        app_path = os.path.join(paths.work, dist_config.app_dir)
        new_app_path = os.path.join(root_directory, dist_config.app_dir)
        commands.copy_files(app_path, root_directory)

        # The spaces are removed from |dist_config.app_product| for the
        # component package path due to a bug in Installer.app that causes the
        # "Show Files" window to be blank if there is a space in a component
        # package name. https://stackoverflow.com/questions/43031272/
        component_pkg_name = '{}.pkg'.format(dist_config.app_product).replace(
            ' ', '')
        component_pkg_path = os.path.join(pkg_paths.work, component_pkg_name)
        component_property_path = _component_property_path(
            pkg_paths, dist_config)
        scripts_path = _create_pkgbuild_scripts(pkg_paths, dist_config)

        commands.run_command([
            'pkgbuild', '--root', root_directory, '--component-plist',
            component_property_path, '--identifier', dist_config.base_bundle_id,
            '--version', dist_config.version, '--install-location',
            '/Applications', '--scripts', scripts_path, component_pkg_path
        ])

        ## The distribution package.

        distribution_path = _productbuild_distribution_path(
            paths, pkg_paths, dist_config, component_pkg_path)

        product_pkg_path = os.path.join(
            pkg_paths.output, '{}.pkg'.format(dist_config.packaging_basename))

        command = [
            'productbuild', '--identifier', dist_config.base_bundle_id,
            '--version', dist_config.version, '--distribution',
            distribution_path, '--package-path', pkg_paths.work, '--sign',
            dist_config.installer_identity
        ]
        if dist_config.notary_user:
            # Assume if the config has notary authentication information that
            # the products will be notarized, which requires a secure
            # timestamp.
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
    pkg_dmg = [
        os.path.join(packaging_dir, 'pkg-dmg'),
        '--verbosity', '0',
        '--tempdir', paths.work,
        '--source', empty_dir,
        '--target', dmg_path,
        '--format', 'UDBZ',
        '--volname', config.app_product,
        '--copy', '{}:/'.format(app_path),
        '--symlink', '/Applications:/ ',
    ]
    # yapf: enable

    if dist.inflation_kilobytes:
        pkg_dmg += [
            '--copy',
            '{}/inflation.bin:/.background/inflation.bin'.format(packaging_dir)
        ]

    if config.is_chrome_branded():
        # yapf: disable
        pkg_dmg += [
            '--icon', os.path.join(packaging_dir, icon_file),
            '--copy',
                '{}/keystone_install.sh:/.keystone_install'.format(packaging_dir),
            '--mkdir', '.background',
            '--copy',
                '{}/chrome_dmg_background.png:/.background/background.png'.format(
                    packaging_dir),
            '--copy', '{}/{}:/.DS_Store'.format(packaging_dir, dsstore_file),
        ]
        # yapf: enable

    commands.run_command(pkg_dmg)

    return dmg_path


def _package_installer_tools(paths, config):
    """Signs and packages all the installer tools, which are not shipped to end-
    users.

    Args:
        paths: A |model.Paths| object.
        config: The |config.CodeSignConfig| object.
    """
    DIFF_TOOLS = 'diff_tools'

    tools_to_sign = parts.get_installer_tools(config)
    chrome_tools = (
        'keystone_install.sh',) if config.is_chrome_branded() else ()
    other_tools = (
        'dirdiffer.sh',
        'dirpatcher.sh',
        'dmgdiffer.sh',
        'pkg-dmg',
    ) + chrome_tools

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


def _intermediate_work_dir_name(dist):
    """Returns the name of an intermediate work directory for a distribution.
    All distributions that can share the same app bundle share the intermediate
    work directory.

    Just about any customization in the distribution will require it to have its
    own app bundle. However, if a distribution is packaged in a PKG (but is not
    also packaged in a DMG), then the brand code is carried in the PKG script,
    and the distribution can share the app bundle of a different distribution
    which is unbranded but for which all the other customizations match.

    Args:
        dist: The |model.Distribution|.

    Returns:
        The work directory name to use.
    """
    customizations = []
    if dist.channel_customize:
        customizations.append('sxs')
    if dist.channel:
        customizations.append(dist.channel)
    else:
        customizations.append('stable')
    if dist.app_name_fragment:
        customizations.append(dist.app_name_fragment)
    if dist.product_dirname:
        customizations.append(dist.product_dirname.replace('/', ' '))
    if dist.creator_code:
        customizations.append(dist.creator_code)
    if dist.branding_code and _include_branding_code_in_app(dist):
        customizations.append(dist.branding_code)
    if dist.inflation_kilobytes:
        customizations.append(str(dist.inflation_kilobytes))

    return '-'.join(customizations)


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
        disable_packaging: Whether all packaging is disabled. If True, the
            unpackaged signed app bundle will be copied to |paths.output|. If
            False, the packaging specified in the distribution will be
            performed.
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
        created_app_bundles = set()
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

                dest_dir = os.path.join(dest_dir,
                                        _intermediate_work_dir_name(dist))

                # Different distributions might share the same underlying app
                # bundle, and if they do, then the _intermediate_work_dir_name
                # function will return the same value. Skip creating another app
                # bundle if that is the case.
                if dest_dir in created_app_bundles:
                    continue
                created_app_bundles.add(dest_dir)

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
                    notary_paths.work,
                    _intermediate_work_dir_name(dist_config.distribution))
                _staple_chrome(notary_paths.replace_work(dest_dir), dist_config)

        # After all apps are optionally notarized, package as required.
        if not disable_packaging:
            uuids_to_package_path = {}
            for dist in config.distributions:
                if dist.branding_code in skip_brands:
                    continue

                dist_config = dist.to_config(config)
                paths = orig_paths.replace_work(
                    os.path.join(
                        notary_paths.work,
                        _intermediate_work_dir_name(dist_config.distribution)))

                if dist.inflation_kilobytes:
                    inflation_path = os.path.join(
                        paths.packaging_dir(config), 'inflation.bin')
                    commands.run_command([
                        'dd', 'if=/dev/urandom', 'of=' + inflation_path,
                        'bs=1000', 'count={}'.format(dist.inflation_kilobytes)
                    ])

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
