#!/usr/bin/python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
package_mac.py - Build and Package Release and Debug libraries for Mac OS X.
"""

import argparse
import glob
import os
import shutil
import sys

from cronet.tools import cr_cronet


def package_mac(out_dir, gn_args, build_config):
  target_dir = out_dir + '/' + build_config
  build_dir = "out/build_mac/" + build_config
  print 'Generating Ninja ' + gn_args
  gn_result = cr_cronet.run('gn gen %s --args=\'%s\'' % (build_dir, gn_args))
  if gn_result != 0:
    return gn_result

  print 'Building ' + build_dir
  build_result = cr_cronet.run('ninja -C %s cronet_package -j200' % build_dir)
  if build_result != 0:
    return build_result

  print 'Copying to ' + target_dir
  if (not os.path.exists(out_dir)):
    shutil.copytree(build_dir + "/cronet", out_dir,
                    ignore=shutil.ignore_patterns('libcronet*'))
  os.mkdir(target_dir)
  for libcronet in glob.glob(build_dir + "/cronet/libcronet*"):
    shutil.copy(libcronet, target_dir)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('out_dir', help='path to output directory')
  options, _ = parser.parse_known_args()

  out_dir = options.out_dir

  # Make sure that the output directory does not exist
  if os.path.exists(out_dir):
    print >>sys.stderr, 'The output directory already exists: ' + out_dir
    return 1

  return package_mac(out_dir, cr_cronet.get_mac_gn_args(True), "opt") or \
         package_mac(out_dir, cr_cronet.get_mac_gn_args(False), "dbg")


if __name__ == '__main__':
  sys.exit(main())
