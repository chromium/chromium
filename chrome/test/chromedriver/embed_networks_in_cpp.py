#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Embeds standalone JavaScript snippets in C++ code.

The script requires the devtools/front_end/toolbox/OverridesUI.js file from
WebKit that lists the preset network conditions to be passed in as the only
argument. The list of network conditions will be written to a C-style string to
be parsed with JSONReader.
"""

import optparse
import re
import subprocess
import sys

import cpp_source

UNLIMITED_THROUGHPUT = ('WebInspector.OverridesSupport'
                        '.NetworkThroughputUnlimitedValue')


def quotizeKeys(s, keys):
  """Returns the string s with each instance of each key wrapped in quotes.

  Args:
    s: a string containing keys that need to be wrapped in quotes.
    keys: an iterable of keys to be wrapped in quotes in the string.
  """
  for key in keys:
    s = re.sub('%s: ' % key, '"%s": ' % key, s)
  return s


def evaluateMultiplications(s):
  """Returns the string s with each bare multiplication evaluated.

  Since the source is JavaScript, which includes bare arithmetic, and the
  output must be JSON format, we must evaluate all expressions.

  Args:
    s: a string containing bare multiplications that need to be evaluated.
  """
  def evaluateBinaryMultiplication(match):
    return str(float(match.group(1)) * float(match.group(2)))

  return re.sub('([0-9\.]+) \* ([0-9\.]+)', evaluateBinaryMultiplication, s)


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--directory', type='string', default='.',
      help='Path to directory where the cc/h files should be created')
  options, args = parser.parse_args()

  networks = '['
  file_name = args[0]
  inside_list = False
  with open(file_name, 'r', encoding='utf-8') as f:
    for line in f:
      if not inside_list:
        if 'WebInspector.OverridesUI._networkConditionsPresets = [' in line:
          inside_list = True
      else:
        if line.strip() == '];':
          inside_list = False
          continue
        line = line.replace(UNLIMITED_THROUGHPUT, "-1")
        networks += line.strip()

  output_dir = 'chrome/test/chromedriver/chrome'
  networks += ']'
  networks = quotizeKeys(networks, ['id', 'title', 'throughput', 'latency'])
  networks = evaluateMultiplications(networks)
  cpp_source.WriteSource('network_list',
                         output_dir,
                         options.directory, {'kNetworks': networks})

  clang_format = ['clang-format', '-i']
  subprocess.Popen(clang_format + ['%s/network_list.cc' % output_dir])
  subprocess.Popen(clang_format + ['%s/network_list.h' % output_dir])


if __name__ == '__main__':
  sys.exit(main())
