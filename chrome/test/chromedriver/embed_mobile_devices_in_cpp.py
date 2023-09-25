#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Embeds standalone JavaScript snippets in C++ code.

The script requires the Source/devtools/front_end/emulated_devices/module.json
file from Blink that lists the known mobile devices to be passed in as the only
argument.  The list of known devices will be written to a C-style string to be
parsed with JSONReader.
"""

import ast
import json
import optparse
import os
import sys

import chrome_paths
import cpp_source

_EMULATED_DEVICES_BEGIN = '// DEVICE-LIST-BEGIN'
_EMULATED_DEVICES_END = '// DEVICE-LIST-END'
_EMULATED_DEVICES_IF = '/* DEVICE-LIST-IF-JS */'
_EMULATED_DEVICES_ELSE = '/* DEVICE-LIST-ELSE'
_EMULATED_DEVICES_ENDIF = 'DEVICE-LIST-END-IF */'

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
  version_file = open(options.version_file, 'r', encoding='utf-8')
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
  with open(file_name, 'r', encoding='utf-8') as f:
    data = f.read()

    # Extract the list from the source file.
    begin_position = data.find(_EMULATED_DEVICES_BEGIN)
    end_position = data.find(_EMULATED_DEVICES_END)
    if begin_position == -1 or end_position == -1:
      print('Could not find list of emulatedDevices in %s' % file_name)
      return 1
    begin_position += len(_EMULATED_DEVICES_BEGIN)
    list_string = '[' + data[begin_position:end_position] + ']'

    # Only used the non-localized strings in the list.
    if_position = list_string.find(_EMULATED_DEVICES_IF)
    while if_position != -1:
      else_position = list_string.find(_EMULATED_DEVICES_ELSE)
      if else_position == -1:
        print('Could not find list of emulatedDevices in %s' % file_name)
        return 1
      else_position += len(_EMULATED_DEVICES_ELSE)
      list_string = list_string[0:if_position] + list_string[else_position::]

      endif_position = list_string.find(_EMULATED_DEVICES_ENDIF)
      if endif_position == -1:
        print('Could not find list of emulatedDevices in %s' % file_name)
        return 1
      list_string = list_string[0:endif_position] + \
          list_string[endif_position + len(_EMULATED_DEVICES_ENDIF)::]

      if_position = list_string.find(_EMULATED_DEVICES_IF)

    # Do a bunch of substitutions to get something parseable by Python.
    list_string = list_string.replace('true', 'True')
    list_string = list_string.replace('false', 'False')
    emulated_devices = ast.literal_eval(list_string)
  for device in emulated_devices:
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
      mobile_emulation = {
        'userAgent': device['user-agent'],
        'deviceMetrics': {
          'width': device['screen']['vertical']['width'],
          'height': device['screen']['vertical']['height'],
          'deviceScaleFactor': device['screen']['device-pixel-ratio'],
          'touch': 'touch' in device['capabilities'],
          'mobile': 'mobile' in device['capabilities'],
        },
        'type': device['type']
      }
      if 'user-agent-metadata' in device:
        client_hints = device['user-agent-metadata']
        mobile_emulation['clientHints'] = {
            'architecture': client_hints['architecture'],
            'bitness': '',
            'platform': client_hints['platform'],
            'platformVersion': client_hints['platformVersion'],
            'model': client_hints['model'],
            'mobile': client_hints['mobile'],
            'wow64': False,
        }
      devices[title] = mobile_emulation

  output_dir = 'chrome/test/chromedriver/chrome'
  cpp_source.WriteSource('mobile_device_list',
                         output_dir,
                         options.directory,
                         {'kMobileDevices': json.dumps(devices)})

if __name__ == '__main__':
  sys.exit(main())
