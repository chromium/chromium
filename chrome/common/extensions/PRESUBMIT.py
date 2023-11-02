# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting extensions.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

USE_PYTHON3 = True

import fnmatch
import os
import re

EXTENSIONS_PATH = os.path.join('chrome', 'common', 'extensions')
DOCS_PATH = os.path.join(EXTENSIONS_PATH, 'docs')
API_PATH = os.path.join(EXTENSIONS_PATH, 'api')
TEMPLATES_PATH = os.path.join(DOCS_PATH, 'templates')
PUBLIC_TEMPLATES_PATH = os.path.join(TEMPLATES_PATH, 'public')
INTROS_PATH = os.path.join(TEMPLATES_PATH, 'intros')
ARTICLES_PATH = os.path.join(TEMPLATES_PATH, 'articles')

def _ReadFile(filename):
  with open(filename) as f:
    return f.read()

def _CheckHeadingIDs(input_api):
  ids_re = re.compile('<h[23].*id=.*?>')
  headings_re = re.compile('<h[23].*?>')
  bad_files = []
  for name in input_api.AbsoluteLocalPaths():
    if not os.path.exists(name):
      continue
    if (fnmatch.fnmatch(name, '*%s*' % INTROS_PATH) or
        fnmatch.fnmatch(name, '*%s*' % ARTICLES_PATH)):
      contents = input_api.ReadFile(name)
      if (len(re.findall(headings_re, contents)) !=
          len(re.findall(ids_re, contents))):
        bad_files.append(name)
  return bad_files

def _CheckLinks(input_api, output_api, results):
  for affected_file in input_api.AffectedFiles():
    name = affected_file.LocalPath()
    absolute_path = affected_file.AbsoluteLocalPath()
    if not os.path.exists(absolute_path):
      continue
    if (fnmatch.fnmatch(name, '%s*' % PUBLIC_TEMPLATES_PATH) or
        fnmatch.fnmatch(name, '%s*' % INTROS_PATH) or
        fnmatch.fnmatch(name, '%s*' % ARTICLES_PATH) or
        fnmatch.fnmatch(name, '%s*' % API_PATH)):
      contents = _ReadFile(absolute_path)
      args = []
      if input_api.platform == 'win32':
        args = [input_api.python_executable]
      args.extend([os.path.join('docs', 'server2', 'link_converter.py'),
                   '-o',
                   '-f',
                   absolute_path])
      output = input_api.subprocess.check_output(
          args,
          cwd=input_api.PresubmitLocalPath(),
          universal_newlines=True)
      if output != contents:
        changes = ''
        for i, (line1, line2) in enumerate(
            zip(contents.split('\n'), output.split('\n'))):
          if line1 != line2:
            changes = ('%s\nLine %d:\n-%s\n+%s\n' %
                (changes, i + 1, line1, line2))
        if changes:
          results.append(output_api.PresubmitPromptWarning(
              'File %s may have an old-style <a> link to an API page. Please '
              'run docs/server2/link_converter.py to convert the link[s], or '
              'convert them manually.\n\nSuggested changes are: %s' %
              (name, changes)))

def _CheckChange(input_api, output_api):
  results = [
      output_api.PresubmitError('File %s needs an id for each heading.' % name)
      for name in _CheckHeadingIDs(input_api)]

  # TODO(kalman): Re-enable this check, or decide to delete it forever. Now
  # that we have multiple directories it no longer works.
  # See http://crbug.com/297178.
  #_CheckLinks(input_api, output_api, results)

  return results

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results += _CheckChange(input_api, output_api)
  return results

def CheckChangeOnCommit(input_api, output_api):
  return _CheckChange(input_api, output_api)
