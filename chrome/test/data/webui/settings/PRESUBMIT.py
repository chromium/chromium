# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

USE_PYTHON3 = True


def _CommonChecks(input_api, output_api):
  # Ignore files in the chromeos/ subfolder.
  EXCLUDE_PATH = input_api.os_path.normpath(
      'chrome/test/data/webui/settings/chromeos/')

  def allow_js(f):
    path = f.LocalPath()
    return path.startswith(EXCLUDE_PATH) or path.endswith('_browsertest.js')

  from web_dev_style import presubmit_support
  return presubmit_support.DisallowNewJsFiles(input_api, output_api,
                                              lambda f: not allow_js(f))


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
