#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates a zip archive with policy template files.
"""

import optparse
import os
import sys

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             os.pardir, os.pardir, os.pardir,
                             'build', 'android', 'gyp'))
from util import build_utils


def main(argv):
  """Pack a list of files into a zip archive.

  Args:
    output: The file path of the zip archive.
    base_dir: Base path of input files.
    languages: Comma-separated list of languages, e.g. en-US,de.
    add: List of files to include in the archive. The language placeholder
         ${lang} is expanded into one file for each language.
  """
  parser = optparse.OptionParser()
  parser.add_option("--output", dest="output")
  parser.add_option("--base_dir", dest="base_dir")
  parser.add_option("--languages", dest="languages")
  parser.add_option("--add", action="append", dest="files", default=[])
  options, args = parser.parse_args(argv[1:])

  # Process file list, possibly expanding language placeholders.
  _LANG_PLACEHOLDER = "${lang}"
  languages = filter(bool, options.languages.split(','))
  file_list = []
  for file_to_add in options.files:
    if (_LANG_PLACEHOLDER in file_to_add):
      for lang in languages:
        file_list.append(file_to_add.replace(_LANG_PLACEHOLDER, lang))
    else:
      file_list.append(file_to_add)

  with build_utils.AtomicOutput(options.output) as f:
    build_utils.DoZip(file_list, f, options.base_dir)


if '__main__' == __name__:
  sys.exit(main(sys.argv))
