# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/extensions/common.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

import sys

PRESUBMIT_VERSION = '2.0.0'


def CheckExternsUpload(input_api, output_api):
  original_sys_path = sys.path
  join = input_api.os_path.join
  src_root = input_api.change.RepositoryRoot()
  try:
    sys.path.append(join(src_root, 'extensions', 'common', 'api'))
    from externs_checker import ExternsChecker
  finally:
    sys.path = original_sys_path

  return ExternsChecker(input_api, output_api).RunChecks()
