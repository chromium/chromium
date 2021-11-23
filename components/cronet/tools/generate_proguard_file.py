#!/usr/bin/env python
#
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Tool that combines a sequence of input proguard files and outputs a single
# proguard file.
#
# The final output file is formed by concatenating all of the
# input proguard files.

import optparse
import sys


def ReadFile(path):
  with open(path, 'rb') as f:
    return f.read()


def main():
  parser = optparse.OptionParser()
  parser.add_option('--output-file',
          help='Output file for the generated proguard file')

  options, input_files = parser.parse_args()

  # Concatenate all the proguard files.
  with open(options.output_file, 'wb') as target:
    for input_file in input_files:
      target.write(ReadFile(input_file))


if __name__ == '__main__':
  sys.exit(main())
