#!/usr/bin/env python
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Simple script to automatically download all current wpr archive files
# for Android WPR record replay tests or upload any newly generated ones.

import argparse
import logging
import os

from upload_download_utils import download
from upload_download_utils import upload
from upload_download_utils import verify_file_exists

STORAGE_BUCKET = 'chrome-wpr-archives'
THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..', '..'))


def _GetReplayTestDirectories():
  directories = [
      os.path.join(CHROMIUM_SRC, 'chrome', 'android', 'feed', 'core',
                   'javatests', 'src', 'org', 'chromium', 'chrome', 'browser',
                   'feed', 'wpr_tests'),
      os.path.join(CHROMIUM_SRC, 'clank', 'javatests', 'src', 'org', 'chromium',
                   'chrome', 'browser', 'wprtests', 'replays')
  ]

  return [d for d in directories if os.path.isdir(d)]


def _is_file_of_interest(f):
  """Filters all wprgo archive file through."""
  return f.endswith('.wprgo')


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('action', choices=['download', 'upload'],
                      help='Which action to perform')
  parser.add_argument('--dry_run', action='store_true',
                      default=False, help='Dry run for uploading')
  args = parser.parse_args()

  if args.action == 'download':
    for d in _GetReplayTestDirectories():
      download(d, _is_file_of_interest,
               'WPR archives', STORAGE_BUCKET)
  else:
    for d in _GetReplayTestDirectories():
      upload(d, _is_file_of_interest,
             'WPR archives', STORAGE_BUCKET, args.dry_run)


if __name__ == '__main__':
  main()
