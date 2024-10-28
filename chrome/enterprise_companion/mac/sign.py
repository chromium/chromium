# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to sign and optionally notarize the Chrome Enterprise Companion App
"""

import sys
import argparse
import subprocess
import logging
import shutil
import os
import tempfile
import stat


def sign(path, identity):
    """Signs an application bundle via codesign(1).

    Args:
        path: The file path of the application bundle to sign.
        identity: The signing identity, see codesign(1) for details.

    Returns:
        The returncode of the codesign utility.
    """
    return subprocess.run([
        'codesign', '-vv', '--sign', identity, '--force', '--timestamp',
        '--options=restrict,library,runtime,kill', path
    ]).returncode


def validate(path):
    """Verifies code signatures via codesign(1).

    Args:
        path: The file path of the application bundle to validate.

    Returns:
        The returncode of the codesign utility, which should be zero if
        validation succeeds.
    """
    return subprocess.run(['codesign', '-v', path]).returncode


def zip(path, output):
    """Recursively zip a directory.

    Args:
        path: The file path to the directory to archive.
        output: A path to write the archive to.

    Returns:
        The returncode of the zip(1) utility.
    """
    path = os.path.normpath(path)
    return subprocess.run([
        'zip', '--recurse-paths', '--quiet',
        os.path.abspath(output),
        os.path.basename(path)
    ],
                          cwd=os.path.dirname(path)).returncode


def notarize(tool_path, file):
    """Submits a notarization to Apple via the notarization tool.

    Submits notarizations to Apple using the notarization tool, which is a
    replacement for notarytool(1) built to work with Google infrastructure.

    Args:
        tool_path: The file path to the notarization tool binary.
        file: The path to the file to submit for notarization.

    Returns:
        The returncode of the notarization tool.
    """
    return subprocess.run([tool_path, "--file", file]).returncode


def staple(file):
    """Attaches tickets for notarized executables to app bundles or disk images.

    Args:
        file: The path to the file to staple. It should already be notarized.

    Returns:
        The returncode of the stapler utility.
    """
    return subprocess.run(['xcrun', 'stapler', 'staple', '-v',
                           file]).returncode


def create_dmg(app_bundle_path, output_dir):
    """Creates an Omaha-compatible DMG for an application bundle via pkg-dmg.

    Creates a DMG suitable for installation via Omaha using the pkg-dmg script
    adjacent to this script. A install script is added to the root of the DMG
    which is invoked by Omaha during updates and installations.

    Args:
        app_bundle_path: The path to the app bundle to package.
        output_dir: A path to a directory in which to write the output DMG.
    """

    def write_install_script(tempdir):
        install_file = os.path.join(tempdir, '.install')
        with open(install_file, 'w') as f:
            f.write('#!/bin/bash\n'
                    r'"$1/ChromeEnterpriseCompanion.app/Contents/MacOS/'
                    r'ChromeEnterpriseCompanion" ${SERVER_ARGS}')
        st = os.stat(install_file)
        os.chmod(install_file,
                 st.st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
        return install_file

    with tempfile.TemporaryDirectory() as tempdir:
        install_script_path = write_install_script(tempdir)
        work_dir = tempfile.mkdtemp(dir=tempdir)
        empty_dir = tempfile.mkdtemp(dir=tempdir)
        return subprocess.run([
            os.path.join(os.path.dirname(sys.argv[0]),
                         'pkg-dmg'), '--verbosity', '0', '--tempdir', work_dir,
            '--source', empty_dir, '--target',
            os.path.join(output_dir,
                         'ChromeEnterpriseCompanion.dmg'), '--format', 'UDBZ',
            '--volname', 'ChromeEnterpriseCompanion', '--copy',
            '{}:/'.format(os.path.normpath(app_bundle_path)),
            '--copy', '{}:/.install'.format(install_script_path)
        ]).returncode


def main(options):
    should_notarize = options.notarization_tool is not None

    # Sign the application bundle.
    if sign(options.input, options.identity) != 0:
        logging.error('Code signing failed')
        return 1
    if validate(options.input) != 0:
        logging.error('Code signing validation failed')
        return 1

    # Optionally notarize and staple the application.
    if should_notarize:
        with tempfile.TemporaryDirectory() as tempdir:
            # Application bundles are directories. They must be zipped for
            # uploading to Apple.
            zip_path = os.path.join(tempdir, 'signed_enterprise_companion.zip')
            zip(options.input, zip_path)
            if notarize(options.notarization_tool, zip_path) != 0:
                logging.error('Failed to notarize %s', zip_path)
                return 1
            if staple(options.input) != 0:
                logging.error('Failed to staple %s', options.input)
                return 1

    # Create a DMG installer.
    if create_dmg(options.input, options.output) != 0:
        logging.error('DMG packaging failed')
        return 1
    # Optionally notarize and staple the DMG.
    if should_notarize:
        dmg_path = os.path.join(options.output,
                                'ChromeEnterpriseCompanion.dmg')
        if notarize(options.notarization_tool, dmg_path) != 0:
            logging.error('Failed to notarize %s', dmg_path)
            return 1
        if staple(dmg_path) != 0:
            logging.error('Failed to staple %s', dmg_path)
            return 1

    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Signing driver for CECA')
    parser.add_argument('--input', type=str, required=True)
    parser.add_argument('--identity', type=str, required=True)
    parser.add_argument('--notarization-tool', type=str, required=False)
    parser.add_argument('--output', type=str, required=True)
    sys.exit(main(parser.parse_args()))
