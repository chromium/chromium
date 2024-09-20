# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to sign and notarize the Chrome Enterprise Companion App
"""

import sys
import argparse
import subprocess
import logging
import shutil
import os


def sign(path, identity):
    return subprocess.run([
        'codesign', '-vv', '--sign', identity, '--force', '--timestamp',
        '--options=restrict,library,runtime,kill',
        path
    ]).returncode


def validate(path):
    return subprocess.run(['codesign', '-v', path]).returncode


def notarize(tool_path, file):
    return subprocess.run([tool_path, "--file", file])


def copy(input, output):
    input = os.path.normpath(input)
    output = os.path.normpath(output)
    shutil.copytree(input, os.path.join(output, os.path.basename(input)))


def main(options):
    if sign(options.input, options.identity) != 0:
        logging.error('Code signing failed')
        return 1
    if validate(options.input) != 0:
        logging.error('Code signing validation failed')
        return 1
    if notarize(options.notarization_tool, options.input) != 0:
        logging.error('Notarization tool failed')
        return 1
    copy(options.input, options.output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Signing driver for CECA')
    parser.add_argument('--input', type=str, required=True)
    parser.add_argument('--identity', type=str, required=True)
    parser.add_argument('--notarization-tool', type=str, required=True)
    parser.add_argument('--output', type=str, required=True)
    sys.exit(main(parser.parse_args()))
