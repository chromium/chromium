# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'


def CheckChange(input_api, output_api):
  # HACK ALERT.
  # CheckPatchFormatted() does not allow for an excluded file list, and the
  # presubmit checks do not honor `.clang-format-ignore` files when formatting
  # diffs. To avoid formatting files in these directories, we temporarily mock
  # AffectedFiles.
  excluded_dirs = [
      input_api.os_path.join(input_api.PresubmitLocalPath(), d) for d in
      (
        # Platform apps are deprecated. No need to format all their test files.
        'platform_apps',

        # These are raw dumps from user data directories. We should not manually
        # modify their contents.
        'profiles',
        'good',

        # Formatting for this dir Coming Soon to a Repo Near You.
        'api_test'
      )
  ]

  original_affected_files = input_api.AffectedFiles

  def filtered_affected_files(include_deletes=True, file_filter=None):
    files = original_affected_files(include_deletes=include_deletes,
                                    file_filter=file_filter)
    filtered = []
    for f in files:
      if not any(f.AbsoluteLocalPath().startswith(d) for d in excluded_dirs):
        filtered.append(f)
    return filtered

  input_api.AffectedFiles = filtered_affected_files

  results = []
  try:
    results += input_api.canned_checks.CheckPatchFormatted(input_api,
                                                           output_api,
                                                           check_js=True,
                                                           check_python=False)
  finally:
    input_api.AffectedFiles = original_affected_files

  # Run eslint.
  try:
    import sys
    old_sys_path = sys.path[:]
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(
        cwd, '..', '..', '..', '..', 'tools')]
    import web_dev_style.presubmit_support
    results += web_dev_style.presubmit_support.CheckStyleESLint(
        input_api, output_api)

  finally:
    sys.path = old_sys_path

  return results
