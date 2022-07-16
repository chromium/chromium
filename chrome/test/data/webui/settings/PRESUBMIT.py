# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

USE_PYTHON3 = True


def _CommonChecks(input_api, output_api):
  results = []

  EXCLUDE_PATH = input_api.os_path.normpath(
      'chrome/test/data/webui/settings/chromeos/')

  def AddedJsFilesFilter(affected_file):
    if affected_file.Action() == 'A':
      # Ignore files in the chromeos/ subfolder.
      if affected_file.LocalPath().startswith(EXCLUDE_PATH):
        return False

      filename = input_api.os_path.basename(affected_file.LocalPath())
      _, extension = input_api.os_path.splitext(filename)
      return extension == '.js'

    return False

  added_js_files = input_api.AffectedFiles(
      include_deletes=False, file_filter=AddedJsFilesFilter)

  for f in added_js_files:
    results += [output_api.PresubmitError(
      'Disallowed JS file found \'%s\'. New test files must be written in '
      'TypeScript instead.' % f.LocalPath())]

  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
