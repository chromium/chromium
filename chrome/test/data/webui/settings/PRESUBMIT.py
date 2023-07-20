# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.



def _CommonChecks(input_api, output_api):
  # Ignore files in the chromeos/ subfolder.
  EXCLUDE_PATH_PREFIX = input_api.os_path.normpath(
      'chrome/test/data/webui/settings/chromeos/')

  # Also exempt top-level browser test definitions files, which must be in JS.
  EXCLUDE_PATH_SUFFIXES = [
      '_browsertest.js',
      '_interactive_ui_tests.js',
  ]

  def allow_js(f):
    path = f.LocalPath()
    if path.startswith(EXCLUDE_PATH_PREFIX):
      return True
    for suffix in EXCLUDE_PATH_SUFFIXES:
      if path.endswith(suffix):
        return True
    return False

  from web_dev_style import presubmit_support
  return presubmit_support.DisallowNewJsFiles(input_api, output_api,
                                              lambda f: not allow_js(f))


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
