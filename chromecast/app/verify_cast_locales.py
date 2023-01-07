#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Ensures that Chromecast developers are notified of locale changes."""

from __future__ import print_function

import argparse
import sys

CAST_LOCALES = [
    'am', 'ar', 'bg', 'bn', 'ca', 'cs', 'da', 'de', 'el', 'en-GB', 'en-US',
    'es-419', 'es', 'et', 'fa', 'fi', 'fil', 'fr', 'gu', 'he', 'hi', 'hr', 'hu',
    'id', 'it', 'ja', 'kn', 'ko', 'lt', 'lv', 'ml', 'mr', 'ms', 'nb', 'nl',
    'pl', 'pt-BR', 'pt-PT', 'ro', 'ru', 'sk', 'sl', 'sr', 'sv', 'sw', 'ta',
    'te', 'th', 'tr', 'uk', 'vi', 'zh-CN', 'zh-TW'
]

SUCCESS_RETURN_CODE = 0
FAILURE_RETURN_CODE = 1


# Chromecast OWNERS need to know if the list of locales used in
# //build/config/locales.gni changes, so that the Chromecast build process
# can be updated accordingly when it does.
#
# This script runs a check to verify that the list of locales maintained in GN
# matches CAST_LOCALES above. If a CL changes that list, it must also change
# CAST_LOCALES in this file to make the Cast trybot pass. This change will
# require adding a //chromecast OWNER to the change, keeping the team aware of
# any locale changes.
def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('locales', type=str, nargs='+',
                      help='Locales from the GN locale list')
  parser.add_argument('--stamp-file', '-s', type=str, required=True,
                      help='The script will stamp this file if successful.')
  args = parser.parse_args()

  if set(CAST_LOCALES) == set(args.locales):
    open(args.stamp_file, 'w')
    return SUCCESS_RETURN_CODE

  # The lists do not match. Compute the difference and log it to the developer.
  removed_locales = set(CAST_LOCALES) - set(args.locales)
  added_locales = set(args.locales) - set(CAST_LOCALES)

  print('CAST_LOCALES no longer matches the locales list from GN!')
  if removed_locales:
    print('These locales have been removed: {}'.format(list(removed_locales)))
  if added_locales:
    print('These locales have been added: {}'.format(list(added_locales)))
  print(('Please update CAST_LOCALES in {file} and add a reviewer from '
         '//chromecast/OWNERS to your CL. ').format(file=__file__))
  return FAILURE_RETURN_CODE


if __name__ == '__main__':
  sys.exit(main())
