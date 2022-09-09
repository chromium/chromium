# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A simple command to answer UAC prompt.

To interact with UAC, we create this standalone program so that the process
can run with the same security token as the desired winlogon.exe, and on the
same desktop that UAC prompts.

Sample usage:
  # Accepts UAC twice, deny once, and then accepts twice with each operation
  # timeouts in 20 seconds.
  python answer_uac.py --timeout=20 --actions=AADAA
"""

import argparse
import logging
import os
import sys

import uac


def _ParseCommandLine():
    """Parse the command line arguments."""
    cmd_parser = argparse.ArgumentParser(
        description='Window UAC prompt handler')

    cmd_parser.add_argument(
        '--actions',
        dest='actions',
        type=str,
        default='A',
        help='How to handle UAC prompt, A for accept, D for deny.')
    cmd_parser.add_argument(
        '--timeout',
        default=30,
        type=float,
        help='Time to wait for each UAC prompt before giving up.')
    cmd_parser.add_argument(
        '--source',
        default='',
        help='Name of the source that triggers UAC, optional (for logging).')
    return cmd_parser.parse_args()


def main():
    flags = _ParseCommandLine()

    logging.info('Command run: %s', sys.argv)

    actions = flags.actions.upper()
    for action in actions:
        if action == 'A':  # Perform action 'Accept'
            logging.info('Next UAC prompt will be accepted.')
            accept = True
        elif action == 'D':  # Perform action 'Deny'
            logging.error('Next UAC prompt will be denied.')
            accept = False
        else:
            logging.error('Unknown action for UAC prompt: [%s]', action)
            continue
        uac.AnswerUpcomingUACPrompt(accept, flags.timeout)


if __name__ == '__main__':
    main()
