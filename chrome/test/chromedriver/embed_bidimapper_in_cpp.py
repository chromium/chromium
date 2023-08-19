#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Embeds standalone JavaScript snippets in C++ code.

Each argument to the script must be a file containing an associated JavaScript
function (e.g., evaluate_script.js should contain an evaluateScript function).
This is called the exported function of the script. The entire script will be
put into a C-style string in the form of an anonymous function which invokes
the exported function when called.
"""

import optparse
import os
import sys

import cpp_source


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--directory', type='string', default='.',
      help='Path to directory where the cc/h js file should be created')
  options, args = parser.parse_args()

  global_string_map = {}
  for js_file in args:
    base_name = os.path.basename(js_file)[:-3].title().replace('_', '')
    func_name = base_name[0].lower() + base_name[1:]
    script_name = 'k%sScript' % base_name
    with open(js_file, 'r', encoding='utf-8') as f:
      contents = f.read()
      global_string_map[script_name] = contents

  cpp_source.WriteSource('bidimapper', 'chrome/test/chromedriver/bidimapper',
                         options.directory, global_string_map)


if __name__ == '__main__':
  sys.exit(main())
