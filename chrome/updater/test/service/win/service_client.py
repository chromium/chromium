# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Calls a function defined in rpc_client.py, which will forward the request
to the test service.

This command assumes the updater test service is already installed on the
system.

Example usages:
    vpython3 service_proxy.py --function=RunAsSystem \
        --args='{"command": "notepad.exe"}'
    vpython3 service_proxy.py --function=AnswerUpcomingUACPrompt \
        --args='{"actions": "A", "wait_child": false, "source": "demo"}'
"""

import argparse
import logging
import json
import sys

import rpc_client


def ParseCommandLine():
    """Parse the command line arguments."""
    cmd_parser = argparse.ArgumentParser(
        description='Updater test service client')

    cmd_parser.add_argument(
        '--function',
        dest='function',
        type=str,
        help='Name of the function to call, defined in rpc_client.py')
    cmd_parser.add_argument('--args',
                            dest='args',
                            type=json.loads,
                            help='Arguments to the function, in json format.')
    return cmd_parser.parse_args()


def main():
    flags = ParseCommandLine()

    if not flags.function:
        logging.error('Must specify a function to call.')
        sys.exit(-1)

    if not hasattr(rpc_client, flags.function):
        logging.error('Function %s is not defined in module rpc_client.',
                      flags.function)
    function = getattr(rpc_client, flags.function)
    result = function(**flags.args)
    logging.error('Function [%s] returned: %s', flags.function, result)
    sys.exit(0)


if __name__ == '__main__':
    main()
