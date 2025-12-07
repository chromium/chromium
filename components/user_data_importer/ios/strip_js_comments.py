# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Strip comments from a JS file."""

import argparse
import os
import shutil
import subprocess
import sys


def parse_args(args):
  """Parses the command-line."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--input', required=True, help='Path to input file')
  parser.add_argument('--output', required=True, help='Path to output file')
  return parser.parse_args(args)


def main(args):
  parsed = parse_args(args)

  with open(parsed.input, 'r') as f:
    with open(parsed.output, 'w') as g:
      for line in f:
        if not line.startswith('//'):
          g.write(line)


if __name__ == '__main__':
  main(sys.argv[1:])
