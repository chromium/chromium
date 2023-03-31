#!/usr/bin/env python3
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates a zip archive with policy template files.
"""

import argparse
import os
import sys

_DIR_SOURCE_ROOT = os.path.join(os.path.dirname(__file__), '..', '..', '..')
sys.path.append(os.path.abspath(os.path.join(_DIR_SOURCE_ROOT, 'build')))
import action_helpers
import zip_helpers


def main():
  """Pack a list of files into a zip archive.

  Args:
    output: The file path of the zip archive.
    base_dir: Base path of input files.
    languages: Comma-separated list of languages, e.g. en-US,de.
    add: List of files to include in the archive. The language placeholder
         ${lang} is expanded into one file for each language.
  """
  parser = argparse.ArgumentParser()
  parser.add_argument("--output", dest="output")
  parser.add_argument("--timestamp",
                      type=int,
                      metavar="TIME",
                      help="Unix timestamp to use for files in the archive")
  parser.add_argument("--base_dir", dest="base_dir")
  parser.add_argument("--languages", dest="languages")
  parser.add_argument("--add", action="append", dest="files", default=[])
  args = parser.parse_args()

  # Process file list, possibly expanding language placeholders.
  _LANG_PLACEHOLDER = "${lang}"
  languages = list(filter(bool, args.languages.split(',')))
  file_list = []
  for file_to_add in args.files:
    if (_LANG_PLACEHOLDER in file_to_add):
      for lang in languages:
        file_list.append(file_to_add.replace(_LANG_PLACEHOLDER, lang))
    else:
      file_list.append(file_to_add)

  with action_helpers.atomic_output(args.output) as f:
    zip_helpers.add_files_to_zip(file_list,
                                 f,
                                 base_dir=args.base_dir,
                                 timestamp=args.timestamp)


if '__main__' == __name__:
  sys.exit(main())
