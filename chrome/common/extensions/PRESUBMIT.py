# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting extensions.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


import fnmatch
import os
import re

PRESUBMIT_VERSION = '2.0.0'

EXTENSIONS_PATH = os.path.join('chrome', 'common', 'extensions')
DOCS_PATH = os.path.join(EXTENSIONS_PATH, 'docs')
TEMPLATES_PATH = os.path.join(DOCS_PATH, 'templates')
INTROS_PATH = os.path.join(TEMPLATES_PATH, 'intros')
ARTICLES_PATH = os.path.join(TEMPLATES_PATH, 'articles')


def _CheckHeadingIDs(input_api):
  ids_re = re.compile('<h[23].*id=.*?>')
  headings_re = re.compile('<h[23].*?>')
  bad_files = []
  for name in input_api.AbsoluteLocalPaths():
    if not os.path.exists(name):
      continue
    if fnmatch.fnmatch(name, '*%s*' % INTROS_PATH) or fnmatch.fnmatch(
        name, '*%s*' % ARTICLES_PATH
    ):
      contents = input_api.ReadFile(name)
      if len(re.findall(headings_re, contents)) != len(
          re.findall(ids_re, contents)
      ):
        bad_files.append(name)
  return bad_files


def CheckChange(input_api, output_api):
  results = [
      output_api.PresubmitError('File %s needs an id for each heading.' % name)
      for name in _CheckHeadingIDs(input_api)
  ]

  return results
