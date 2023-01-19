#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test driver for integration tests of Chrome's mini_installer.

The most simple way to run these tests is from the context of a build output
directory (e.g., src\out\Rel).
"""

import argparse
import os
import sys

# Add typ to the path if it's not already there, then import it.
CUR_DIR = os.path.dirname(os.path.realpath(__file__))
SRC_DIR = os.path.dirname(os.path.dirname(os.path.dirname(CUR_DIR)))
TYP_DIR = os.path.join(SRC_DIR, 'third_party', 'catapult', 'third_party',
                       'typ')
if TYP_DIR not in sys.path:
    sys.path.insert(0, TYP_DIR)
del SRC_DIR, TYP_DIR

import typ

from argument_parser import ArgumentParser


def _prepare_env_for_subprocesses(parser, args):
    """Populates environment variables with values given to this process on its
        command line.

    typ runs tests in child processes. It does not pass args defined by this
    test suite, so here we stuff the args the suite cares about into the
    environment so they can be pulled out by test modules that need them.

    Args:
        parser: The ArgumentParser used for this process.
        args: The arguments parsed from this process's command line.
    """
    if args.force_clean:
        os.environ['CMI_FORCE_CLEAN'] = '1'
    if args.output_dir != parser.get_default('output_dir'):
        os.environ['CMI_OUTPUT_DIR'] = args.output_dir
    if args.installer_path != parser.get_default('installer_path'):
        os.environ['CMI_INSTALLER_PATH'] = args.installer_path
    if args.previous_version_installer_path != parser.get_default(
            'previous_version_installer_path'):
        os.environ['CMI_PREVIOUS_VERSION_INSTALLER_PATH'] = \
            args.previous_version_installer_path
    if args.chromedriver_path != parser.get_default('chromedriver_path'):
        os.environ['CMI_CHROMEDRIVER_PATH'] = args.chromedriver_path
    if args.config != parser.get_default('config'):
        os.environ['CMI_CONFIG'] = args.config


def main(args):
    host = typ.Host()
    runner = typ.Runner(host)
    parser = ArgumentParser(host)
    parser.prog = os.path.basename(sys.argv[0])
    parser.description = __doc__
    parser.formatter_class = argparse.RawDescriptionHelpFormatter
    runner.parse_args(
        parser=parser,
        argv=args,
        isolate=['installer_test.*'],  # InstallerTest must be serialized.
        top_level_dir=CUR_DIR,
        retry_limit=3,  # Retry failures by default since the tests are flaky.
    )
    if parser.exit_status is not None:
        return parser.exit_status

    # Stuff args into environment vars for use by child procs.
    _prepare_env_for_subprocesses(parser, runner.args)

    try:
        return runner.run()[0]
    except KeyboardInterrupt:
        sys.stderr.write("interrupted, exiting")
        return 130


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
