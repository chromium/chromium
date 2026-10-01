#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--in_folder', required=True)
    parser.add_argument('--ts_files', nargs='+', required=True)
    parser.add_argument('--cc_files', nargs='+', required=True)
    parser.add_argument('--out_file', required=True)
    args = parser.parse_args()

    # Read all C++ files into a single string.
    cc_content = ''
    for cc_file in args.cc_files:
        with open(cc_file, 'r', encoding='utf-8') as f:
            cc_content += f.read()

    # Check if each TS file is referenced.
    ts_files = list(
        map(lambda f: os.path.join(args.in_folder, f), args.ts_files))
    missing_files = []
    for ts_file in ts_files:
        basename = os.path.basename(ts_file).replace('.ts', '.js')
        if re.search(rf'\b{basename}\b', cc_content) is None:
            missing_files.append(basename)

    if missing_files:
        print(
            'Error: The following Mocha test files are not referenced in the provided C++ files:',
            file=sys.stderr)
        print(' ', '\n  '.join(missing_files), file=sys.stderr)
        print(
            'Each Mocha test file should be referenced in at least one C++ test.',
            file=sys.stderr)
        sys.exit(1)

    with open(args.out_file, 'w', newline='', encoding='utf-8') as f:
        f.write('OK')


if __name__ == '__main__':
    main()
