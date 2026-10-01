#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper script for running WebXR Android browser tests on an emulator.

The WebXR tests require specific arguments when run on an Android emulator,
such as using the validating command decoder to avoid host OpenGL ES
driver translation stalls.

This script wraps the `android_browsertests` test runner and appends the
necessary flags by default.
"""

import argparse
import pathlib
import subprocess
import sys


def get_test_executable():
    return 'android_browsertests'


def create_argument_parser():
    parser = argparse.ArgumentParser(
        description='This is a wrapper script around %s for running WebXR tests '
        'on an Android emulator. To view help for the underlying runner, run '
        '`%s --help`.' % (get_test_executable(), get_test_executable()))

    parser.add_argument(
        '--gtest_filter',
        default='*WebXr*',
        help='Test filter to run. Defaults to "*WebXr*".')
    parser.add_argument(
        '--use-cmd-decoder',
        default='validating',
        dest='cmd_decoder',
        help='Command decoder to use. Defaults to "validating" for emulator '
        'compatibility.')

    return parser


def main():
    parser = create_argument_parser()
    args, rest_args = parser.parse_known_args()

    test_executable = (
        pathlib.Path(__file__).resolve().parent / get_test_executable()
    )

    cmd = [str(test_executable)]
    if args.cmd_decoder:
        cmd.append(f'--use-cmd-decoder={args.cmd_decoder}')
    if args.gtest_filter:
        cmd.append(f'--gtest_filter={args.gtest_filter}')
    cmd.extend(rest_args)

    sys.exit(subprocess.call(cmd))


if __name__ == '__main__':
    main()
