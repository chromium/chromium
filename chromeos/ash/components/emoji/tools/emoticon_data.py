# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import sys

_SCRIPT_DIR = os.path.realpath(os.path.dirname(__file__))
_CHROME_SOURCE = os.path.realpath(
    os.path.join(_SCRIPT_DIR, *[os.path.pardir] * 5))
sys.path.append(os.path.join(_CHROME_SOURCE, 'build'))

import action_helpers

# Set of unicode characters that do not render with fonts available on ChromeOS
# TODO(b:267370102) add font(s) such that there are no more invalid characters
INVALID_CHARACTERS = set([
    '\u2688\u0325',
    '\ufe52\ufe20',
    '\u0644',
])


def isValidEmoticon(string):
    for symbol in INVALID_CHARACTERS:
        if symbol in string:
            return False
    return True


def process_emoticon_data(metadata):
    """Produce the emoticon data to be consumed by the emoji picker.
    Args:
        metadata (list(dict)): list of emoticon group data.

    Returns:
        list(dict): list of readily used emoticon groups.
    """
    return [{
        'group':
        group['group'],
        'emoji': [{
            'base': {
                'string': emoticon['value'],
                'name': emoticon['description'],
            },
        } for emoticon in group['emoticon']
                  if isValidEmoticon(emoticon['value'])]
    } for group in metadata]


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--metadata',
                        required=True,
                        help='emoji metadata ordering file as JSON')
    parser.add_argument('--output',
                        required=True,
                        help='output JSON file path')
    options = parser.parse_args(args)

    metadata_file = options.metadata
    output_file = options.output

    # Parse emoticon ordering data.
    metadata = []
    with open(metadata_file, 'r') as file:
        metadata = json.load(file)

    emoticon_data = process_emoticon_data(metadata)

    # Write output file atomically in utf-8 format.
    with action_helpers.atomic_output(output_file) as tmp_file:
        tmp_file.write(
            json.dumps(emoticon_data,
                       separators=(',', ':'),
                       ensure_ascii=False).encode('utf-8'))


if __name__ == '__main__':
    main(sys.argv[1:])
