# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def _CheckSvgsOptimized(input_api, output_api):
  results = []
  try:
    import sys
    old_sys_path = sys.path[:]
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(cwd, '..', 'tools')]
    from resources import svgo_presubmit
    results += svgo_presubmit.CheckOptimized(input_api, output_api)
  finally:
    sys.path = old_sys_path
  return results


def _CheckWebDevStyle(input_api, output_api):
  results = []
  try:
    import sys
    old_sys_path = sys.path[:]
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(cwd, '..', 'tools')]
    from web_dev_style import presubmit_support
    results += presubmit_support.CheckStyle(input_api, output_api)
  finally:
    sys.path = old_sys_path
  return results


def _CheckNotFatalUntilAdoption(input_api, output_api):
  results = []
  try:
    import sys
    old_sys_path = sys.path[:]
    sys.path.append(input_api.change.RepositoryRoot())
    from build.ios import presubmit_support

    # Filter components to only those consumed by //ios/web_view.
    # Using NotFatalUntil annotations within web_view code is crucial,
    # given that this code is used by embedders which need time to
    # find and resolve issues before crashing CHECKS are added.
    def PathFilter(affected_file):
      path = affected_file.UnixLocalPath()
      return (path.startswith('components/password_manager/') or
              path.startswith('components/autofill/'))

    results.extend(presubmit_support.CheckNotFatalUntilAdoption(
        input_api, output_api, path_filter=PathFilter))
  finally:
    sys.path = old_sys_path
  return results


def _CommonChecks(input_api, output_api):
  results = []
  results += _CheckSvgsOptimized(input_api, output_api)
  results += _CheckWebDevStyle(input_api, output_api)
  results += _CheckNotFatalUntilAdoption(input_api, output_api)
  results += input_api.canned_checks.CheckPatchFormatted(input_api, output_api,
                                                         check_js=True,
                                                         check_python=False)
  return results
