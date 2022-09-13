# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates a header containing a dictionary from locales to
# accept language strings from chromium's .xtb files.  It is not very
# robust at the moment, and makes some assumptions about the format of
# the files, including at least the following:
#   * assumes necessary data is contained only with files of the form
#     components/strings/components_locale_settings_${LANG}.xtb
#   * assumes ${LANG} is identified in the lang attribute of the root
#     element of the file's xml data
#   * assumes that there is only one relevant element with the
#     IDS_ACCEPT_LANGUAGES attribute

from __future__ import print_function

import os
import re
import sys
from xml.etree import ElementTree

STRINGS_DIR = sys.argv[2] + 'components/strings/'

# pylint: disable=inconsistent-return-statements
def extract_accept_langs(filename):
  tree = ElementTree.parse(STRINGS_DIR + filename).getroot()
  for child in tree:
    if child.get('id') == 'IDS_ACCEPT_LANGUAGES':
      return tree.get('lang'), child.text
# pylint: enable=inconsistent-return-statements

def gen_accept_langs_table():
  accept_langs_list = [extract_accept_langs(filename)
    for filename in os.listdir(STRINGS_DIR)
    if re.match(r'components_locale_settings_\S+.xtb', filename)]
  return dict(accept_langs for accept_langs in accept_langs_list
    if accept_langs)

HEADER = "static NSDictionary* const acceptLangs = @{"
def LINE(locale, accept_langs):
  return '  @"' + locale + '" : @"' + accept_langs + '",'
FOOTER = "};"

def main():
  with open(sys.argv[1] + "/accept_languages_table.h", "w+") as f:
    print(HEADER, file=f)
    for (locale, accept_langs) in gen_accept_langs_table().items():
      print(LINE(locale, accept_langs), file=f)
    print(FOOTER, file=f)

if __name__ == "__main__":
  main()
