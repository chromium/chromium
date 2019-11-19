# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os.path
import sys
import subprocess

# Wraps chrome/tools/build/mac/verify_order for the GN build so that it can
# write a stamp file, rather than operate as a postbuild.

if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description='A wrapper around verify_order that writes a stamp file.')
  parser.add_argument('--stamp', action='store', type=str,
      help='Touch this stamp file on success.')

  args, unknown_args = parser.parse_known_args()

  this_script_dir = os.path.dirname(sys.argv[0])
  rv = subprocess.check_call(
      [ os.path.join(this_script_dir, 'verify_order') ] + unknown_args)

  if rv == 0 and args.stamp:
    if os.path.exists(args.stamp):
      os.unlink(args.stamp)
    open(args.stamp, 'w+').close()

  sys.exit(rv)
