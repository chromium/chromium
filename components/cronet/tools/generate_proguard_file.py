#!/usr/bin/env python
#
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Tool that combines a sequence of input proguard files and outputs a single
# proguard file.
#
# The final output file is formed by concatenating all of the
# input proguard files.

import argparse
import sys


def ReadFile(path):
  with open(path, 'rb') as f:
    return f.read()


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output-file',
          help='Output file for the generated proguard file')

  args, input_files = parser.parse_known_args()

  # Concatenate all the proguard files.
  with open(args.output_file, 'wb') as target:
    for input_file in input_files:
      target.write(ReadFile(input_file))


if __name__ == '__main__':
  sys.exit(main())
