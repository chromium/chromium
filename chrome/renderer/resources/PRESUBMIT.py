# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'


def _ImportWebDevStyle(input_api):
  try:
    import sys
    old_sys_path = sys.path[:]
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(cwd, '..', '..', '..', 'tools')]
    from web_dev_style import presubmit_support
  finally:
    sys.path = old_sys_path
  return presubmit_support


def CheckWebDevStyle(input_api, output_api):
  presubmit_support = _ImportWebDevStyle(input_api)
  return presubmit_support.CheckStyle(input_api, output_api)


def CheckPatchFormatted(input_api, output_api):
  results = input_api.canned_checks.CheckPatchFormatted(input_api, output_api,
                                                         check_js=True)
  return results
