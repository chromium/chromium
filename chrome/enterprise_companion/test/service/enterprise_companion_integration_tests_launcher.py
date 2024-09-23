# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Launches the enterprise companion integration tests, possibly as root.

This script ensures that the enterprise companion integration tests are run at
the appropriate privilege level. On POSIX, the given command is run as root via
passwordless-sudo. On Windows, the command is executed directly.

All arguments provided to this program will be used to reconstruct the command
line for the child process. For example,
    vpython3 enterprise_companion_integration_test_launcher.py \
        --gtest_shuffle --test-launcher-bot-mode
will launch a process with command line:
    enterprise_companion_integration_tests --gtest_shuffle \
        --test-launcher-bot-mode

On POSIX, this script must be run by a user with passwordless sudo.
"""
import argparse
import distutils
import logging
import os
import subprocess
import sys


def ParseCommandLine():
    """Parse the command line arguments."""
    cmd_parser = argparse.ArgumentParser()
    cmd_parser.add_argument('--test-output-dir',
                            dest='test_output_dir',
                            type=str,
                            required=True,
                            help='The directory containing test outputs.')
    return cmd_parser.parse_known_args()


def GetIntegrationTestsExeName():
    if os.name == 'posix':
        return 'enterprise_companion_integration_tests'
    elif os.name == 'nt':
        return 'enterprise_companion_integration_tests.exe'
    else:
        logging.error("Unsupported os: %s", os.name)
        sys.exit(1)


def main():
    flags, remaining_args = ParseCommandLine()

    exe_name = GetIntegrationTestsExeName()

    # Find the location of the test exe. shutil.which() looks suitable for this,
    # only if https://bugs.python.org/issue24505 is closed. For now, use the
    # one from distutils module.
    command = distutils.spawn.find_executable(exe_name)
    if not command:
        logging.error('Cannot find command: %s', exe_name)
        sys.exit(-2)

    # Command may be in relative path. Make it absolute so that the RPC server
    # can find it.
    command = os.path.abspath(command)

    command_line = []
    if os.name == 'posix':
        command_line = ['sudo', '--non-interactive']
    command_line += [command] + remaining_args
    logging.error('Full command line: %s',
                  subprocess.list2cmdline(command_line))

    exit_code = subprocess.run(command_line).returncode

    # Artifacts created by the test binary will be owned by root. Ensure
    # ownership returns to the current user.
    if os.name == 'posix':
        subprocess.run([
            "sudo", "--non-interactive", "chown", "-R",
            os.environ.get('USER'), flags.test_output_dir
        ])

    sys.exit(exit_code)


if __name__ == '__main__':
    main()
