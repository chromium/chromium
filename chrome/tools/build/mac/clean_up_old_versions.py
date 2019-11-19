# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import shutil
import sys


def CleanUpOldVersions(args):
  versions_dir = args.versions_dir
  if os.path.exists(versions_dir):
    for version in os.listdir(versions_dir):
      if version not in args.keep:
        path = os.path.join(versions_dir, version)
        if os.path.isdir(path) and not os.path.islink(path):
          shutil.rmtree(path)
        else:
          os.unlink(path)

  for path in args.delete:
    if os.path.exists(path):
      shutil.rmtree(path)

  open(args.stamp, 'w').close()
  os.utime(args.stamp, None)


def Main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--versions-dir',
      required=True,
      help='The path where versioned directories are stored')
  parser.add_argument(
      '--keep',
      action='append',
      default=[],
      help=('The names of items to keep in the `--versions-dir`. '
            'Can be specified multiple times.'))
  parser.add_argument(
      '--delete',
      action='append',
      default=[],
      help=('Unconditionally deletes the tree at this path. Can be '
            'specified multiple times.'))
  parser.add_argument(
      '--stamp', required=True, help='Path to write the stamp file.')
  args = parser.parse_args()

  CleanUpOldVersions(args)
  return 0

if __name__ == '__main__':
  sys.exit(Main())
