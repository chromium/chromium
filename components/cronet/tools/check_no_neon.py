#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""check_no_neon.py - Check modules do not contain ARM Neon instructions."""

import argparse
import os
import sys

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

sys.path.insert(0, os.path.join(REPOSITORY_ROOT, 'build/android/gyp'))
from util import build_utils  # pylint: disable=wrong-import-position


def main(args):
  parser = argparse.ArgumentParser(
      description='Check modules do not contain ARM Neon instructions.')
  parser.add_argument('objdump', metavar='path/to/ARM/objdump')
  parser.add_argument('objects', metavar='files/to/check/*.o')
  parser.add_argument('--stamp', help='Path to touch on success.')
  opts = parser.parse_args(args)
  ret = os.system(opts.objdump + ' -d --no-show-raw-insn ' +
      opts.objects + ' | grep -q "vld[1-9]\\|vst[1-9]"')

  # Non-zero exit code means no neon.
  if ret and opts.stamp:
    build_utils.Touch(opts.stamp)
  return ret


if __name__ == '__main__':
  sys.exit(0 if main(sys.argv[1:]) != 0 else -1)
