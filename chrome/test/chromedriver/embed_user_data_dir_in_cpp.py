#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Embeds Chrome user data files in C++ code."""

import optparse
import os
import sys

import cpp_source


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--directory', type='string', default='.',
      help='Path to directory where the cc/h  file should be created')
  options, args = parser.parse_args()

  global_string_map = {}
  for data_file in args:
    title = os.path.basename(os.path.splitext(data_file)[0]).title()
    var_name = 'k' + title.replace('_', '')
    with open(data_file, 'r', encoding='utf-8') as f:
      contents = f.read()
    global_string_map[var_name] = contents

  cpp_source.WriteSource('user_data_dir', 'chrome/test/chromedriver/chrome',
                         options.directory, global_string_map)


if __name__ == '__main__':
  sys.exit(main())
