# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The pipeline module orchestrates the entire signing process, which includes:
    1. Code signing the application bundle and all of its nested code.
    2. Producing a packaged DMG.
    3. Signing the DMG.
"""

import os.path

from signing import commands, model, notarize, parts, signing


def _sign_app(paths, config, dest_dir):
    """Does signing of an updater app bundle, which is moved into |dest_dir|.

    Args:
        paths: A |model.Paths| object.
        config: A |config.CodeSignConfig|.
        dest_dir: The directory into which the product will be placed when
            the operations are completed.
    """
    commands.copy_files(os.path.join(paths.input, config.app_dir), paths.work)
    commands.copy_files(
        os.path.join(paths.input, "{.app_product}Util".format(config)),
        paths.work)
    parts.sign_all(paths, config)
    commands.make_dir(dest_dir)
    commands.move_file(os.path.join(paths.work, config.app_dir),
                       os.path.join(dest_dir, config.app_dir))
    commands.move_file(
        os.path.join(paths.work, "{.app_product}Util".format(config)),
        os.path.dirname(dest_dir))


def _package_and_sign_dmg(paths, config):
    """Packages, signs, and verifies a DMG for a signed build product.

    Args:
        paths: A |model.Paths| object.
        config: The |config.CodeSignConfig| object.

    Returns:
        The path to the signed DMG file.
    """
    dmg_path = _package_dmg(paths, config)
    product = model.CodeSignedProduct(dmg_path,
                                      config.packaging_basename,
                                      sign_with_identifier=True)
    signing.sign_part(paths, config, product)
    signing.verify_part(paths, product)
    return dmg_path


def _package_dmg(paths, config):
    """Packages an Updater application bundle into a DMG.

    Args:
        paths: A |model.Paths| object.
        config: The |config.CodeSignConfig| object.

    Returns:
        A path to the produced DMG file.
    """
    dmg_path = os.path.join(paths.output,
                            '{}.dmg'.format(config.packaging_basename))
    app_path = os.path.join(paths.work, config.app_dir)
    empty_dir = os.path.join(paths.work, 'empty')
    commands.make_dir(empty_dir)
    pkg_dmg = [
        os.path.join(paths.input, config.packaging_dir, 'signing', 'pkg-dmg'),
        '--verbosity',
        '0',
        '--tempdir',
        paths.work,
        '--source',
        empty_dir,
        '--target',
        dmg_path,
        '--format',
        'UDBZ',
        '--volname',
        config.app_product,
        '--copy',
        '{}:/'.format(app_path),
        '--copy',
        '{}/chrome/updater/.install:/.keystone_install'.format(paths.input),
    ]
    commands.run_command(pkg_dmg)
    return dmg_path


def _package_and_sign_pkg(paths, config):
    """Packages, signs, and verifies a PKG.

    Args:
        paths: A |model.Paths| object.
        config: The |config.CodeSignConfig| object.

    Returns:
        The path to the signed PKG file.
    """
    pkg_path = os.path.join(paths.output,
                            '{}.pkg'.format(config.packaging_basename))
    args = [
        'pkgbuild',
        '--root',
        os.path.join(paths.work, config.app_dir),
        '--install-location',
        os.path.join('/Library/Application Support', config.company_name,
                     config.app_product, 'PkgStaging',
                     '%s.app' % config.app_product),
        '--scripts',
        os.path.join(paths.input, config.packaging_dir, 'signing', 'pkg'),
        '--timestamp',
        pkg_path,
    ]
    if config.installer_identity:
        args.extend([
            '--sign',
            config.installer_identity,
        ])
    commands.run_command(args)
    return pkg_path


def _package_zip(paths, config):
    """Packages an Updater application bundle into a ZIP.

    Args:
        paths: A |model.Paths| object.
        config: The |config.CodeSignConfig| object.

    Returns:
        A path to the produced ZIP file.
    """
    zip_path = os.path.join(paths.output,
                            '{}.zip'.format(config.packaging_basename))
    prep_dir = os.path.join(paths.work, 'zip_prep')
    commands.make_dir(prep_dir)
    commands.copy_files(os.path.join(paths.work, config.app_dir), prep_dir)
    commands.copy_files('{}/chrome/updater/.install'.format(paths.input),
                        prep_dir)
    commands.zip(zip_path, prep_dir)
    return zip_path


def sign_all(orig_paths,
             config,
             disable_packaging=False,
             skip_brands=[],
             channels=[]):
    """Code signs, packages, and signs the package, placing the result into
    |orig_paths.output|. |orig_paths.input| must contain the products to
    customize and sign.

    Args:
        orig_paths: A |model.Paths| object.
        config: The |config.CodeSignConfig| object.
        disable_packaging: Ignored.
        skip_brands: Ignored.
        channels: Ignored.
    """
    with commands.WorkDirectory(orig_paths) as notary_paths:
        # First, sign and optionally submit the notarization requests.
        uuid = None
        with commands.WorkDirectory(orig_paths) as paths:
            dest_dir = os.path.join(notary_paths.work,
                                    config.packaging_basename)
            _sign_app(paths, config, dest_dir)

            if config.notarize.should_notarize():
                zip_file = os.path.join(notary_paths.work,
                                        config.packaging_basename + '.zip')
                commands.run_command([
                    'zip', '--recurse-paths', '--symlinks', '--quiet',
                    zip_file, config.app_dir
                ],
                                     cwd=dest_dir)
                uuid = notarize.submit(zip_file, config)

        # Wait for the app notarization result to come back and staple.
        if config.notarize.should_wait():
            for _ in notarize.wait_for_results([uuid], config):
                pass  # We are only waiting for a single notarization.
            if config.notarize.should_staple():
                notarize.staple_bundled_parts(
                    # Only staple to the outermost app.
                    parts.get_parts(config)[-1:],
                    notary_paths.replace_work(
                        os.path.join(notary_paths.work,
                                     config.packaging_basename)))

        # Package.
        commands.move_file(
            os.path.join(notary_paths.work,
                         "{.app_product}Util".format(config)),
            orig_paths.output)
        package_paths = orig_paths.replace_work(
            os.path.join(notary_paths.work, config.packaging_basename))
        _package_zip(package_paths, config)
        dmg_path = _package_and_sign_dmg(package_paths, config)
        pkg_path = _package_and_sign_pkg(package_paths, config)

        # Notarize the packages, then staple.
        if config.notarize.should_notarize():
            uuid_to_path = {}
            uuid_to_path[notarize.submit(pkg_path, config)] = pkg_path
            uuid_to_path[notarize.submit(dmg_path, config)] = dmg_path
            for uuid in notarize.wait_for_results(uuid_to_path.keys(), config):
                notarize.staple(uuid_to_path[uuid])
