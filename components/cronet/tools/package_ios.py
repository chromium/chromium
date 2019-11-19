#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
package_ios.py - Build and Package Release and Debug frameworks for iOS.
"""

import argparse
import os
import shutil
import sys

from cronet.tools import cr_cronet


def package_ios_framework_using_gn(out_dir):
  print 'Building Cronet Dynamic Framework...'

  # Package all builds in the output directory.
  os.makedirs(out_dir)
  build_dir = ''
  for (is_release, build_config, gn_extra_args) in \
        [(False, 'Debug', ''),
         (True, 'Release', 'enable_stripping=true ')]:
    for (target_device, target_cpu, additional_cpu) in \
        [('os', 'arm', 'arm64'),
         ('simulator', 'x86', 'x64')]:
      target_dir = '%s-iphone%s' % (build_config, target_device)
      build_dir = os.path.join("out", "build_ios", target_dir)
      gn_args = cr_cronet.get_ios_gn_args(is_release, target_cpu) + \
                'additional_target_cpus = ["%s"] ' % additional_cpu

      print 'Generating ninja ' + target_dir
      gn_result = cr_cronet.gn(build_dir, gn_args + gn_extra_args)
      if gn_result != 0:
        return gn_result

      print 'Building ' + target_dir
      build_result = cr_cronet.build(build_dir, 'cronet_package')
      if build_result != 0:
        return build_result

      # Copy framework.
      shutil.copytree(os.path.join(build_dir, 'Cronet.framework'),
          os.path.join(out_dir, 'Dynamic', target_dir, 'Cronet.framework'))
      # Copy symbols.
      shutil.copytree(os.path.join(build_dir, 'Cronet.dSYM'),
          os.path.join(out_dir, 'Dynamic', target_dir, 'Cronet.framework.dSYM'))
      # Copy static framework.
      shutil.copytree(os.path.join(build_dir, 'Static', 'Cronet.framework'),
          os.path.join(out_dir, 'Static', target_dir, 'Cronet.framework'))

  # Copy common files from last built package.
  package_dir = os.path.join(build_dir, 'cronet')
  shutil.copy2(os.path.join(package_dir, 'AUTHORS'), out_dir)
  shutil.copy2(os.path.join(package_dir, 'LICENSE'), out_dir)
  shutil.copy2(os.path.join(package_dir, 'VERSION'), out_dir)
  # Copy the headers.
  shutil.copytree(os.path.join(build_dir,
                               'Cronet.framework', 'Headers'),
                  os.path.join(out_dir, 'Headers'))
  print 'Cronet framework is packaged into %s' % out_dir


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('out_dir', help='path to output directory')
  options, _ = parser.parse_known_args()

  out_dir = options.out_dir

  # Make sure that the output directory does not exist
  if os.path.exists(out_dir):
    print >>sys.stderr, 'The output directory already exists: ' + out_dir
    return 1

  return package_ios_framework_using_gn(out_dir)


if __name__ == '__main__':
  sys.exit(main())
