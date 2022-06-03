#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Embeds standalone JavaScript snippets in C++ code.

The script requires the Source/devtools/front_end/emulated_devices/module.json
file from Blink that lists the known mobile devices to be passed in as the only
argument.  The list of known devices will be written to a C-style string to be
parsed with JSONReader.
"""

from __future__ import print_function

import json
import optparse
import os
import re
import subprocess
import sys

import chrome_paths
import cpp_source


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--version-file', type='string',
      default=os.path.join(chrome_paths.GetSrc(), 'chrome', 'VERSION'),
      help='Path to Chrome version file')
  parser.add_option(
      '', '--directory', type='string', default='.',
      help='Path to directory where the cc/h files should be created')
  options, args = parser.parse_args()

  # The device userAgent string may contain '%s', which should be replaced with
  # current Chrome version. First we read the version file.
  version_parts = ['MAJOR', 'MINOR', 'BUILD', 'PATCH']
  version = []
  version_file = open(options.version_file, 'r')
  for part in version_parts:
    # The version file should have 4 lines, with format like MAJOR=63
    components = version_file.readline().split('=')
    if len(components) != 2 or components[0].strip() != part:
      print('Bad version file')
      return 1
    version.append(components[1].strip())
  # Join parts of version together using '.' as separator
  version = '.'.join(version)

  devices = {}
  file_name = args[0]
  inside_list = False
  with open(file_name, 'r') as f:
    emulated_devices = json.load(f)
  extensions = emulated_devices['extensions']
  for extension in extensions:
    if extension['type'] == 'emulated-device':
      device = extension['device']
      title = device['title']
      titles = [title]
      # For 'iPhone 6/7/8', also add ['iPhone 6', 'iPhone 7', 'iPhone 8'] for
      # backward compatibility.
      if '/' in title:
        words = title.split()
        for i in range(len(words)):
          if '/' in words[i]:
            # Only support one word containing '/'
            break
        tokens = words[i].split('/')
        for token in tokens:
          words[i] = token
          titles.append(' '.join(words))
      for title in titles:
        devices[title] = {
          'userAgent': device['user-agent'].replace('%s', version),
          'width': device['screen']['vertical']['width'],
          'height': device['screen']['vertical']['height'],
          'deviceScaleFactor': device['screen']['device-pixel-ratio'],
          'touch': 'touch' in device['capabilities'],
          'mobile': 'mobile' in device['capabilities'],
        }

  output_dir = 'chrome/test/chromedriver/chrome'
  cpp_source.WriteSource('mobile_device_list',
                         output_dir,
                         options.directory,
                         {'kMobileDevices': json.dumps(devices)})

  clang_format = ['clang-format', '-i']
  subprocess.Popen(clang_format + ['%s/mobile_device_list.cc' % output_dir])
  subprocess.Popen(clang_format + ['%s/mobile_device_list.h' % output_dir])


if __name__ == '__main__':
  sys.exit(main())
