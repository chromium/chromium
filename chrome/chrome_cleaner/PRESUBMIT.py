# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting chrome/chrome_cleaner/

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

def _CheckBuildFilesHaveExplicitVisibility(input_api, output_api):
  """Checks that all BUILD.gn files have a file-level 'visibility' directive.

  We require all build files under //chrome/chrome_cleaner to have visibility
  restrictions to enforce that changes under this directory cannot affect
  Chrome. This lets us safely cherry-pick changes that affect only this
  directory to the stable branch when necessary to fit the Chrome Cleanup
  Tool's release schedule.
  """
  results = []
  files_without_visibility = []
  def IsBuildFile(f):
    local_path = input_api.os_path.normcase(f.LocalPath())
    return local_path.endswith(input_api.os_path.normcase('BUILD.gn'))
  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=IsBuildFile):
    lines = (l.strip() for l in input_api.ReadFile(f).split('\n'))
    for l in lines:
      # First line that isn't blank, a comment, or an import statement should
      # be a visibility directive. We don't check the content of the directive
      # so that maintainers have flexibility to make changes. The important
      # thing is to make sure visibility isn't completely forgotten.
      if l and not l.startswith('#') and not l.startswith('import'):
        if not l.startswith('visibility'):
          files_without_visibility.append(f.LocalPath())
        # No need to check the rest of the file.
        break
  if files_without_visibility:
    results.append(output_api.PresubmitPromptWarning(
        'These build files under //chrome/chrome_cleaner should have a '
        '"visibility" directive after any imports:',
        items=files_without_visibility))
  return results


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(_CheckBuildFilesHaveExplicitVisibility(input_api, output_api))
  return results


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results
