# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The pipeline module orchestrates the entire signing process, which includes:
    1. Code signing the application bundle and all of its nested code.
    2. Producing a packaged DMG.
    3. Signing the DMG.
"""

import os.path

from . import commands, model, notarize, parts, signing


def _sign_app(paths, config, dest_dir):
    """Does signing of an updater app bundle, which is moved into |dest_dir|.

    Args:
        paths: A |model.Paths| object.
        config: A |config.CodeSignConfig|.
        dest_dir: The directory into which the product will be placed when
            the operations are completed.
    """
    commands.copy_files(os.path.join(paths.input, config.app_dir), paths.work)
    parts.sign_all(paths, config)
    commands.make_dir(dest_dir)
    commands.move_file(
        os.path.join(paths.work, config.app_dir),
        os.path.join(dest_dir, config.app_dir))


def _package_and_sign_dmg(paths, config):
    """Packages, signs, and verifies a DMG for a signed build product.

    Args:
        paths: A |model.Paths| object.
        config: The |config.CodeSignConfig| object.

    Returns:
        The path to the signed DMG file.
    """
    dmg_path = _package_dmg(paths, config)
    product = model.CodeSignedProduct(
        dmg_path, config.packaging_basename, sign_with_identifier=True)
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
        '{}/chrome/updater/.install:/'.format(paths.input),
    ]
    commands.run_command(pkg_dmg)
    return dmg_path


def sign_all(orig_paths,
             config,
             disable_packaging=False,
             do_notarization=True,
             skip_brands=[]):
    """Code signs, packages, and signs the package, placing the result into
    |orig_paths.output|. |orig_paths.input| must contain the products to
    customize and sign.

    Args:
        orig_paths: A |model.Paths| object.
        config: The |config.CodeSignConfig| object.
        disable_packaging: Ignored.
        do_notarization: If True, the signed application bundle will be sent for
            notarization by Apple. The resulting notarization ticket will then
            be stapled. The stapled application will be packaged in the DMG and
            then the DMG itself will be notarized and stapled.
        skip_brands: Ignored.
    """
    with commands.WorkDirectory(orig_paths) as notary_paths:
        # First, sign and optionally submit the notarization requests.
        uuid = None
        with commands.WorkDirectory(orig_paths) as paths:
            dest_dir = os.path.join(notary_paths.work,
                                    config.packaging_basename)
            _sign_app(paths, config, dest_dir)

            if do_notarization:
                zip_file = os.path.join(notary_paths.work,
                                        config.packaging_basename + '.zip')
                commands.run_command([
                    'zip', '--recurse-paths', '--symlinks', '--quiet',
                    zip_file, config.app_dir
                ],
                                     cwd=dest_dir)
                uuid = notarize.submit(zip_file, config)

        # Wait for the app notarization result to come back and staple.
        if do_notarization:
            for _ in notarize.wait_for_results([uuid], config):
                pass  # We are only waiting for a single notarization.
            notarize.staple_bundled_parts(
                parts.get_parts(config),
                notary_paths.replace_work(
                    os.path.join(notary_paths.work,
                                 config.packaging_basename)))

        # Package.
        dmg_path = _package_and_sign_dmg(
            orig_paths.replace_work(
                os.path.join(notary_paths.work, config.packaging_basename)),
            config)

        # Notarize the package, then staple.
        if do_notarization:
            for _ in notarize.wait_for_results(
                [notarize.submit(dmg_path, config)], config):
                pass  # We are only waiting for a single notarization.
            notarize.staple(dmg_path)
