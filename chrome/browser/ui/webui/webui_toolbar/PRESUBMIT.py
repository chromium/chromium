# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for chrome/browser/ui/webui/webui_toolbar/."""

PRESUBMIT_VERSION = '2.0.0'

_RELEVANT_PATHS = (
  r'^chrome/browser/ui/webui/webui_toolbar/(?:scripts/|PRESUBMIT)',
  r'^chrome/browser/about_flags\.cc$',
  r'^tools/perf/core/(?:bot_platforms\.py|shard_maps/)',
)


def CheckWebUIToolbarFlagsSynced(input_api, output_api):
  """Checks that about_flags.cc and perf configs match enabled.sh."""
  if input_api.is_windows:
    return []

  if not input_api.no_diffs:

    def file_filter(affected_file):
      return input_api.FilterSourceFile(
        affected_file, files_to_check=_RELEVANT_PATHS
      )

    if not any(
      input_api.AffectedFiles(include_deletes=True, file_filter=file_filter)
    ):
      return []

  script_path = input_api.os_path.join(
    input_api.PresubmitLocalPath(), 'scripts', 'generate_chrome_flags.py'
  )
  if not input_api.os_path.exists(script_path):
    return []

  proc = input_api.subprocess.Popen(
    [input_api.python3_executable, script_path, '--check'],
    cwd=input_api.change.RepositoryRoot(),
    stdout=input_api.subprocess.PIPE,
    stderr=input_api.subprocess.PIPE,
    text=True,
  )
  stdout, stderr = proc.communicate()
  if proc.returncode == 0:
    return []

  message = (
    stderr.strip()
    or stdout.strip()
    or f'generate_chrome_flags.py --check failed (exit {proc.returncode}).'
  )
  if (
    'Out of sync with enabled.sh:' in message
    and 'chrome/browser/about_flags.cc' not in message
  ):
    return [output_api.PresubmitPromptOrNotify(message)]
  return [output_api.PresubmitError(message)]
