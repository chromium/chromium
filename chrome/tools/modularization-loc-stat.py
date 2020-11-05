#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates stats on modularization efforts. Stats include:
- Percentage of added lines in modularized files over legacy ones.
- The top 50 contributors to the modularized files.
"""

import argparse
import subprocess
import sys
from collections import OrderedDict
from collections import defaultdict

_M12N_DIRS = [
    'chrome/browser',
    'components',
]

_LEGACY_DIR = 'chrome/android'

def _gen_stat(dates):
    # Each CL is output in the following format:
    #
    # #:thanhdng:2020-08-17:Use vector icons for zero state file results
    #
    # 118     98      chrome/browser/ui/app_list/file_icon_util.cc
    # 2       1       chrome/browser/ui/app_list/file_icon_util.h
    # 0       20      chrome/browser/ui/app_list/file_icon_util_unittest.cc
    #
    # i.e.:
    #
    # #:author:commit-date:subject
    #
    # added-lines    deleted-lines  file-path1
    # added-lines    deleted-lines  file-path2
    # ...

    command = ['git', 'log', '--numstat', '--no-renames',
               '--format=#:%al:%cs:%s', '--after='+dates[0],
               '--before='+dates[1], 'chrome', 'components']
    try:
        proc = subprocess.Popen(command,
                                bufsize=1,  # buffered mode
                                stdout=subprocess.PIPE,
                                universal_newlines=True)
    except subprocess.SubprocessError as e:
        print(f'{command} failed with code {e.returncode}.', file=sys.stderr)
        print(f'\nSTDERR: {e.stderr}', file=sys.stderr)
        print(f'\nSTDOUT: {e.stdout}', file=sys.stderr)
        raise

    author_stat = defaultdict(int)
    total_m12n = 0
    total_legacy = 0
    prev_msg_len = 0
    revert_cl = False
    for raw_line in proc.stdout:
        if raw_line.isspace():
            continue
        line = raw_line.strip()
        if line.startswith('#'):  # patch summary line
            _, author, commit_date, *subject = line.split(':', 4)
            revert_cl = (subject[0].startswith('Revert') or
                           subject[0].startswith('Reland'))
        else:
            if revert_cl or not line.endswith('.java'):
                continue

            # Do not take into account the number of deleted lines, which can
            # turn the overall changes to negative. If a class was renamed,
            # for instance, what's deleted is added somewhere else, so counting
            # only for addition works. Other kinds of deletion will be ignored.
            added, _deleted, path = line.split()
            diff = int(added)
            if _is_m12n_path(path):
                total_m12n += diff
                author_stat[author] += diff
            elif _is_legacy_path(path):
                total_legacy += diff

        msg = f'\rProcessing {commit_date} by {author}'
        _print_progress(msg, prev_msg_len)
        prev_msg_len = len(msg)

    _print_progress('Processing complete', prev_msg_len)
    print('\n')

    percentage = 100.0 * total_m12n / (total_m12n + total_legacy)
    print(f'# of lines added in modularized files: {total_m12n}')
    print(f'# of lines added in legacy files: {total_legacy}')
    print(f'% of lines landing in modularized files: {percentage:2.2f}%')

    # Shows the top 50 contributors to modularized files.
    print('\nTop contributors:')
    print('No  lines    %    author')
    ranks = OrderedDict(
        sorted(author_stat.items(), key=lambda x:x[1], reverse=True))
    for rank, author in enumerate(list(ranks.keys())[:50], 1):
        lines = ranks[author]
        if lines == 0:
            break
        ratio = 100 * lines / total_m12n
        print(f'{rank:2d} {lines:6d} {ratio:5.1f}  {author}')

def _is_m12n_path(path):
    for prefix in _M12N_DIRS:
        if path.startswith(prefix):
            return True
    return False

def _is_legacy_path(path):
    return path.startswith(_LEGACY_DIR)

def _print_progress(msg, prev_msg_len):
    msg_len = len(msg)

    # Add spaces to remove the previous progress output completely.
    if msg_len < prev_msg_len:
        msg += ' ' * (prev_msg_len - msg_len)
    print(msg, end='\r')


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Generates LOC stats for modularization effort.")
    parser.add_argument(
        '--date',
        required=True,
        type=str,
        metavar=('<date-from>', '<date-to>'),
        nargs=2,
        help='date range (YYYY-MM-DD)~(YYYY-MM-DD)')
    args = parser.parse_args()
    _gen_stat(args.date)
