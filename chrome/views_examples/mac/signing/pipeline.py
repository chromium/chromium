# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The pipeline module orchestrates the entire signing process, which includes:
    1. Code signing the application bundle and all of its nested code.
"""

import os.path

from signing import commands, notarize, parts


async def _sign_app(paths, config, dest_dir):
    """Does signing of a Views Examples app bundle, which is moved into
    |dest_dir|.

    Args:
        paths: A |model.Paths| object.
        config: A |config.CodeSignConfig|.
        dest_dir: The directory into which the product will be placed when
            the operations are completed.
    """
    commands.copy_files(os.path.join(paths.input, config.app_dir), paths.work)
    await parts.sign_all(paths, config)
    commands.make_dir(dest_dir)
    commands.move_file(
        os.path.join(paths.work, config.app_dir),
        os.path.join(dest_dir, config.app_dir),
    )


def _package_zip(paths, config):
    """Packages a Views Examples application bundle into a ZIP.

    Args:
        paths: A |model.Paths| object. The signed app bundle must be in
            |paths.work|.
        config: The |config.CodeSignConfig| object.

    Returns:
        A path to the produced ZIP file.
    """
    zip_path = os.path.join(
        paths.output, '{}.zip'.format(config.packaging_basename)
    )

    # If the file already exists, delete it so we avoid updating an old file
    # rather than creating a new one as intended.
    commands.delete_file_if_exists(zip_path)

    commands.run_command(
        [
            'zip',
            '-9',
            '--recurse-paths',
            '--symlinks',
            '--quiet',
            zip_path,
            config.app_dir,
        ],
        cwd=paths.work,
    )
    return zip_path


async def sign_all(
    orig_paths, config, disable_packaging=False, skip_brands=[], channels=[]
):
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
        dest_dir = os.path.join(notary_paths.work, config.packaging_basename)

        # First, sign and optionally submit the notarization request.
        with commands.WorkDirectory(orig_paths) as paths:
            await _sign_app(paths, config, dest_dir)

            if config.notarize.should_notarize():
                zip_file = os.path.join(
                    notary_paths.work, config.packaging_basename + '.zip'
                )
                commands.run_command(
                    [
                        'zip',
                        '--recurse-paths',
                        '--symlinks',
                        '--quiet',
                        zip_file,
                        config.app_dir,
                    ],
                    cwd=dest_dir,
                )
                await notarize.submit(zip_file, config)

        # |signed_paths.work| holds the signed (and possibly notarized) app.
        signed_paths = notary_paths.replace_work(dest_dir)

        if config.notarize.should_staple():
            notarize.staple_bundled_parts(
                # Only staple to the outermost app.
                parts.get_parts(config)[-1:],
                signed_paths,
            )

        # Package into |orig_paths.output|. This must happen before leaving the
        # WorkDirectory, which deletes |dest_dir|.
        _package_zip(signed_paths, config)
