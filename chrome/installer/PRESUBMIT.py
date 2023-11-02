# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

USE_PYTHON3 = True
PRESUBMIT_VERSION = '2.0.0'

def CheckBreakingInstallerVersionBumpNeeded(input_api, output_api):
  files = []
  breaking_version_installer_updated = False

  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    breaking_version_installer_updated |= (f.LocalPath() ==
    'chrome/installer/setup/last_breaking_installer_version.cc')
    if (f.LocalPath() == 'chrome/installer/mini_installer/chrome.release' or
        f.LocalPath().startswith('chrome/test/mini_installer')):
      files.append(f.LocalPath())

  if files and not breaking_version_installer_updated:
    return [output_api.PresubmitPromptWarning('''
Update chrome/installer/setup/last_breaking_installer_version.cc if the changes
found in the following files might break make downgrades not possible beyond
this browser's version.''', items=files)]

  if not files and breaking_version_installer_updated:
    return [output_api.PresubmitPromptWarning('''
No installer breaking changes detected but
chrome/installer/setup/last_breaking_installer_version.cc was updated. Please
update chrome/installer/PRESUBMIT.py if more files need to be watched for
breaking installer changes.''')]

  return []
