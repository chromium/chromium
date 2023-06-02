# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run the Chrome WebUI presubmit scripts on our test javascript.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools, and see
https://chromium.googlesource.com/chromium/src/+/main/styleguide/web/web.md
for the rules we're checking against here.
"""

import os



def GetPathsToPrepend(input_api):
  web_dev_style_path = input_api.os_path.join(
      input_api.change.RepositoryRoot(),
      'tools')
  return [input_api.PresubmitLocalPath(), web_dev_style_path]

def RunWithPrependedPath(prepended_path, fn, *args):
  import sys
  old_path = sys.path

  try:
    sys.path = prepended_path + old_path
    return fn(*args)
  finally:
    sys.path = old_path

def CheckChangeOnUpload(input_api, output_api):
  def go():
    results = []
    results.extend(_CommonChecks(input_api, output_api))
    return results
  return RunWithPrependedPath(GetPathsToPrepend(input_api), go)

def CheckChangeOnCommit(input_api, output_api):
  def go():
    results = []
    results.extend(_CommonChecks(input_api, output_api))
    return results
  return RunWithPrependedPath(GetPathsToPrepend(input_api), go)

def _CommonChecks(input_api, output_api):
  resources = input_api.PresubmitLocalPath()

  def _html_css_js_resource(p):
    return p.endswith(('.js')) and p.startswith(resources)

  def is_resource(maybe_resource):
    return _html_css_js_resource(maybe_resource.AbsoluteLocalPath())

  from web_dev_style import js_checker

  results = []
  results.extend(js_checker.JSChecker(
      input_api, output_api, file_filter=is_resource).RunChecks())

  return results
