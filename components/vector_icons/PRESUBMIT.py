# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for components/vector_icons.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckIconNames(input_api, output_api):
  affected_icons = []
  for f in input_api.AffectedFiles(include_deletes=False):
    if f.Action() == 'A' and f.LocalPath().endswith('.icon'):
      # Extract icon name from filename (e.g., 'menu.icon' -> 'menu')
      icon_name = input_api.os_path.splitext(
          input_api.os_path.basename(f.LocalPath()))[0]
      affected_icons.append((f.LocalPath(), icon_name, None))

  if not affected_icons:
    return []

  # Import the shared checker
  import sys
  old_sys_path = sys.path[:]
  try:
    sys.path.append(
        input_api.os_path.join(input_api.PresubmitLocalPath(), '..', '..',
                               'tools', 'resources', 'icon_checker'))
    import icon_checker
    return icon_checker.CheckIcons(input_api, output_api, affected_icons)
  finally:
    sys.path = old_sys_path
