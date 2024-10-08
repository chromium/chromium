# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# [VPYTHON:BEGIN]
# python_version: "3.8"
# wheel: <
#   name: "infra/python/wheels/pywin32/${vpython_platform}"
#    version: "version:300"
# >
# [VPYTHON:END]
"""Run the given command as the standard user.

All arguments provided to this program will be used to reconstruct the command
line for the child process. For example,
    vpython3 run_command_as_standard_user.py --command notepad "hello world.txt"
will launch a process with command line:
    notepad "hello world.txt"

This command must be run as an elevated user.
"""

import argparse
import distutils
import logging
import os
import subprocess
import sys
import threading
import time

import rpc_client
import updater_test_service_control


def ParseCommandLine():
    """Parse the command line arguments."""
    cmd_parser = argparse.ArgumentParser(description='Run command as user')

    cmd_parser.add_argument('--command',
                            dest='command',
                            type=str,
                            help='The command to run.')
    return cmd_parser.parse_known_args()


def LogToSTDERR(title, output):
    if not output:
        return

    logging.error('%s %s starts %s', '=' * 30, title, '=' * 30)

    # Directly dump the output to STDERR so we don't have logging prefix each
    # line, to make it easier to read.
    sys.stderr.write(output)

    logging.error('%s  %s ends  %s', '=' * 30, title, '=' * 30)


def ExcludeUpdaterPathsFromWindowsDefender():
    """Put Updater paths into the Windows Defender exclusion list.

    Once in a while, Windows Defender flags the updater binaries as malware,
    which leads to test failures. Stop Windows Defender scanning the paths that
    updater could work on.
    """
    paths_to_exclude = [
        '%ProgramFiles%', '%ProgramFiles(x86)%', '%LocalAppData%'
    ]
    logging.info('Excluding %s from Windows Defender.', paths_to_exclude)

    quote_path_if_needed = lambda p: p if p.startswith('"') else '"' + p + '"'
    subprocess.call([
        'powershell.exe', 'Add-MpPreference', '-ExclusionPath',
        ', '.join([quote_path_if_needed(p) for p in paths_to_exclude])
    ])


def KeepAliveThread():
    """Function logs periodically to notify parent this process is still alive.

    Swarming test infra monitors test sub-process console activities. If there
    is no console output for the extended period of time, it infers that the
    test is not responding and kills the process. Note only STDERR will be
    actually output to the console with current setup.
    """
    for i in range(180):
        time.sleep(60)
        logging.error(
            "%s: still waiting for test sub-process to complete, sleep 60s ...",
            i)


def main():
    flags, remaining_args = ParseCommandLine()

    if not flags.command:
        logging.error('Must specify a command to run.')
        sys.exit(-1)

    # Find the location of the command. shutil.which() looks suitable for this,
    # only if https://bugs.python.org/issue24505 is closed. For now, use the
    # one from distutils module.
    command = distutils.spawn.find_executable(flags.command)
    if not command:
        logging.error('Cannot find command: %s', flags.command)
        sys.exit(-2)

    # Command may be in relative path. Make it absolute so that the RPC server
    # can find it.
    command = os.path.abspath(command)

    # RunAsStandardUser() takes a full command line string (as it is forwarded
    # to the underlying win32 implementation). We have sys.argv, but the value
    # is already processed by shell. It is possible that the reconstructed
    # command line is skewed (for example, expansion of environment variable),
    # but hopefully this works well enough in all real scenarios.
    command_line = subprocess.list2cmdline([command] + remaining_args)
    logging.error('Full command line: %s', command_line)

    ExcludeUpdaterPathsFromWindowsDefender()

    keep_alive_thread = threading.Thread(target=KeepAliveThread)
    keep_alive_thread.setDaemon(True)  # Do not block process on exit.
    keep_alive_thread.start()

    with updater_test_service_control.OpenService():
        pid, exit_code, stdout, stderr = rpc_client.RunAsStandardUser(
            command_line)
        if pid is None:
            logging.error('Failed to launch command: %s', command_line)
            sys.exit(-3)
        LogToSTDERR('STDOUT', stdout)
        if exit_code != 0:
            LogToSTDERR('STDERR', stderr)
        sys.exit(exit_code)


if __name__ == '__main__':
    main()
