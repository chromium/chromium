#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import urllib.request

SHEET_URL = 'https://docs.google.com/spreadsheets/u/1/d/1d3iAOAnojp4_WrPky9exz1-mjkeulOJVUav5QYG99MQ/export'
UNPROCESSED_REQUIRED_COVERAGE_TESTS_TAB = '2008870403'
ACTIONS_TAB = '1864725389'
ENUMS_TAB = '1470912273'
DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data')


def main():
    urllib.request.urlretrieve(
        f'{SHEET_URL}?gid={UNPROCESSED_REQUIRED_COVERAGE_TESTS_TAB}&format=tsv',
        os.path.join(DATA_DIR, 'coverage_required.tsv'))
    urllib.request.urlretrieve(f'{SHEET_URL}?gid={ACTIONS_TAB}&format=tsv',
                               os.path.join(DATA_DIR, 'actions.tsv'))
    urllib.request.urlretrieve(f'{SHEET_URL}?gid={ENUMS_TAB}&format=tsv',
                               os.path.join(DATA_DIR, 'enums.tsv'))


if __name__ == '__main__':
    sys.exit(main())
