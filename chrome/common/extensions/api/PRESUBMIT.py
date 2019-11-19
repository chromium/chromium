# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/extensions/common.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

import sys


def _CheckExterns(input_api, output_api):
  original_sys_path = sys.path

  join = input_api.os_path.join
  api_root = input_api.PresubmitLocalPath()
  src_root = join(api_root, '..', '..', '..', '..')
  try:
    sys.path.append(join(src_root, 'extensions', 'common', 'api'))
    from externs_checker import ExternsChecker
  finally:
    sys.path = original_sys_path

  externs_root = join(src_root, 'third_party', 'closure_compiler', 'externs')

  api_pair_names = {
    'accessibility_private.json': 'accessibility_private.js',
    'activity_log_private.json': 'activity_log_private.js',
    'autofill_private.idl': 'autofill_private.js',
    'developer_private.idl': 'developer_private.js',
    'bookmark_manager_private.json': 'bookmark_manager_private.js',
    'command_line_private.json': 'command_line_private.js',
    'file_manager_private.idl': 'file_manager_private.js',
    'file_system_provider.idl': 'file_system_provider.js',
    'input_method_private.json': 'input_method_private.js',
    'language_settings_private.idl': 'language_settings_private.js',
    'media_player_private.json': 'media_player_private.js',
    'passwords_private.idl': 'passwords_private.js',
    'quick_unlock_private.idl': 'quick_unlock_private.js',
    'resources_private.idl': 'resources_private.js',
    'safe_browsing_private.idl': 'safe_browsing_private.js',
    'settings_private.idl': 'settings_private.js',
    'system_private.json': 'system_private.js',
    'users_private.idl': 'users_private.js',
    # TODO(rdevlin.cronin): Add more!
  }
  normpath = input_api.os_path.normpath
  api_pairs = {
      normpath(join(api_root, k)):
          normpath(join(externs_root, v)) for k, v in api_pair_names.items()
  }

  return ExternsChecker(input_api, output_api, api_pairs).RunChecks()


def CheckChangeOnUpload(input_api, output_api):
  return _CheckExterns(input_api, output_api)
