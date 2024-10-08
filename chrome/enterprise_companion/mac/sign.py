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


def sign(path, identity):
    return subprocess.run([
        'codesign', '-vv', '--sign', identity, '--force', '--timestamp',
        '--options=restrict,library,runtime,kill',
        path
    ]).returncode


def validate(path):
    return subprocess.run(['codesign', '-v', path]).returncode


def notarize(tool_path, file):
    return subprocess.run([tool_path, "--file", file]).returncode


def create_dmg(app_bundle_path, output_dir):
    with tempfile.TemporaryDirectory() as tempdir:
        work_dir = tempfile.mkdtemp(dir=tempdir)
        empty_dir = tempfile.mkdtemp(dir=tempdir)
        return subprocess.run([
            os.path.join(os.path.dirname(sys.argv[0]),
                         'pkg-dmg'), '--verbosity', '0', '--tempdir', work_dir,
            '--source', empty_dir, '--target',
            os.path.join(output_dir, 'ChromeEnterpriseCompanion.dmg'),
            '--format', 'UDBZ', '--volname', 'ChromeEnterpriseCompanion',
            '--copy', '{}:/'.format(app_bundle_path)
        ]).returncode


def main(options):
    if sign(options.input, options.identity) != 0:
        logging.error('Code signing failed')
        return 1
    if validate(options.input) != 0:
        logging.error('Code signing validation failed')
        return 1
    if options.notarization_tool is not None:
        if notarize(options.notarization_tool, options.input) != 0:
            logging.error('Notarization tool failed')
            return 1
    if create_dmg(options.input, options.output) != 0:
        logging.error('DMG packaging failed')
        return 1
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Signing driver for CECA')
    parser.add_argument('--input', type=str, required=True)
    parser.add_argument('--identity', type=str, required=True)
    parser.add_argument('--notarization-tool', type=str, required=False)
    parser.add_argument('--output', type=str, required=True)
    sys.exit(main(parser.parse_args()))
