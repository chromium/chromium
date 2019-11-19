# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a mini_installer with a lower version than an existing one."""

import argparse
import subprocess
import sys


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--alternate_version_generator',
      help='Path to alternate_version_generator.')
  parser.add_argument('--mini_installer', help='Path to input mini_installer')
  parser.add_argument('--out', help='Path to the generated mini_installer.')
  parser.add_argument('--path_7za', help='Path to 7za.exe')
  args = parser.parse_args()
  assert args.alternate_version_generator
  assert args.mini_installer
  assert args.out
  assert args.path_7za

  return subprocess.call([
      args.alternate_version_generator,
      '--force',
      '--previous',
      '--mini_installer=' + args.mini_installer,
      '--out=' + args.out,
      '--7za_path=' + args.path_7za,
      ])


if '__main__' == __name__:
  sys.exit(main())
