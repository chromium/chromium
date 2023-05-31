# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for files in chrome/browser/ui/webui.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


def _CheckWebDevStyle(input_api, output_api):
  results = []
  try:
    import sys
    old_sys_path = sys.path
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(cwd, '..', '..', '..', '..', 'tools')]
    import web_dev_style.presubmit_support
    results = web_dev_style.presubmit_support.CheckStyle(input_api, output_api)
  finally:
    sys.path = old_sys_path
  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CheckWebDevStyle(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CheckWebDevStyle(input_api, output_api)
