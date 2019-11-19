# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for src/components/cronet.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os

def _PyLintChecks(input_api, output_api):
  pylint_checks = input_api.canned_checks.GetPylint(input_api, output_api,
          extra_paths_list=_GetPathsToPrepend(input_api), pylintrc='pylintrc')
  return input_api.RunTests(pylint_checks)


def _GetPathsToPrepend(input_api):
  current_dir = input_api.PresubmitLocalPath()
  chromium_src_dir = input_api.os_path.join(current_dir, '..', '..')
  return [
    input_api.os_path.join(chromium_src_dir, 'components'),
    input_api.os_path.join(chromium_src_dir, 'tools', 'perf'),
    input_api.os_path.join(chromium_src_dir, 'build', 'android'),
    input_api.os_path.join(chromium_src_dir, 'build', 'android', 'gyp'),
    input_api.os_path.join(chromium_src_dir,
        'mojo', 'public', 'tools', 'bindings', 'pylib'),
    input_api.os_path.join(chromium_src_dir, 'net', 'tools', 'net_docs'),
    input_api.os_path.join(chromium_src_dir, 'tools'),
    input_api.os_path.join(chromium_src_dir, 'third_party'),
    input_api.os_path.join(chromium_src_dir,
        'third_party', 'catapult', 'telemetry'),
    input_api.os_path.join(chromium_src_dir,
        'third_party', 'catapult', 'devil'),
    input_api.os_path.join(chromium_src_dir,
        'third_party', 'catapult', 'common', 'py_utils'),
  ]


def _PackageChecks(input_api, output_api):
  """Verify API classes are in org.chromium.net package, and implementation
  classes are not in org.chromium.net package."""
  api_file_pattern = input_api.re.compile(
      r'^components/cronet/android/api/.*\.(java|template)$')
  impl_file_pattern = input_api.re.compile(
      r'^components/cronet/android/java/.*\.(java|template)$')
  api_package_pattern = input_api.re.compile(r'^package (?!org.chromium.net;)')
  impl_package_pattern = input_api.re.compile(r'^package org.chromium.net;')

  source_filter = lambda path: input_api.FilterSourceFile(path,
      white_list=[r'^components/cronet/android/.*\.(java|template)$'])

  problems = []
  for f in input_api.AffectedSourceFiles(source_filter):
    local_path = f.LocalPath()
    for line_number, line in f.ChangedContents():
      if (api_file_pattern.search(local_path)):
        if (api_package_pattern.search(line)):
          problems.append(
            '%s:%d\n    %s' % (local_path, line_number, line.strip()))
      elif (impl_file_pattern.search(local_path)):
        if (impl_package_pattern.search(line)):
          problems.append(
            '%s:%d\n    %s' % (local_path, line_number, line.strip()))

  if problems:
    return [output_api.PresubmitError(
        'API classes must be in org.chromium.net package, and implementation\n'
        'classes must not be in org.chromium.net package.',
        problems)]
  else:
    return []


def _RunToolsUnittests(input_api, output_api):
  return input_api.canned_checks.RunUnitTestsInDirectory(
      input_api, output_api, '.', [ r'^tools_unittest\.py$'])


def _ChangeAffectsCronetTools(change):
  """ Returns |true| if the change may affect Cronet tools. """

  for path in change.LocalPaths():
    if path.startswith(os.path.join('components', 'cronet', 'tools')):
      return True
  return False


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_PyLintChecks(input_api, output_api))
  results.extend(_PackageChecks(input_api, output_api))
  if _ChangeAffectsCronetTools(input_api.change):
    results.extend(_RunToolsUnittests(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  return _RunToolsUnittests(input_api, output_api)
