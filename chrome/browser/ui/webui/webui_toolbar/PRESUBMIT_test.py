# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest
from unittest import mock

import PRESUBMIT

# Append Chromium repository root to import `PRESUBMIT_test_mocks.py`.
sys.path.append(
  os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', '..')
  )
)
from PRESUBMIT_test_mocks import MockAffectedFile
from PRESUBMIT_test_mocks import MockInputApi
from PRESUBMIT_test_mocks import MockOutputApi

_ENABLED_SH = 'chrome/browser/ui/webui/webui_toolbar/scripts/enabled.sh'
_TOOLBAR_CC = 'chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.cc'


class CheckWebUIToolbarFlagsSyncedTest(unittest.TestCase):
  def _run(self, rel_paths, returncode=0, stderr='', is_windows=False):
    input_api = MockInputApi()
    input_api.is_windows = is_windows
    input_api.presubmit_local_path = os.path.dirname(os.path.abspath(__file__))
    input_api.files = [MockAffectedFile(p, ['changed']) for p in rel_paths]
    proc = mock.Mock(returncode=returncode)
    proc.communicate.return_value = ('', stderr)
    with mock.patch.object(
      input_api.subprocess, 'Popen', return_value=proc
    ) as mock_popen:
      results = PRESUBMIT.CheckWebUIToolbarFlagsSynced(
        input_api, MockOutputApi()
      )
    return results, mock_popen

  def testSkipsOnWindows(self):
    results, mock_popen = self._run([_ENABLED_SH], is_windows=True)
    self.assertEqual(results, [])
    mock_popen.assert_not_called()

  def testSkipsUnrelatedToolbarFiles(self):
    results, mock_popen = self._run([_TOOLBAR_CC])
    self.assertEqual(results, [])
    mock_popen.assert_not_called()

  def testPassesWhenAllSynced(self):
    results, mock_popen = self._run([_ENABLED_SH])
    self.assertEqual(results, [])
    mock_popen.assert_called_once()

  def testErrorsWhenAboutFlagsOutOfSync(self):
    stderr = 'Out of sync with enabled.sh:\n  chrome/browser/about_flags.cc'
    results, _ = self._run([_ENABLED_SH], returncode=1, stderr=stderr)
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].type, 'error')
    self.assertIn('chrome/browser/about_flags.cc', results[0].message)

  def testPromptsOrNotifiesWhenOnlyPerfConfigsOutOfSync(self):
    stderr = 'Out of sync with enabled.sh:\n  tools/perf/core/bot_platforms.py'
    results, _ = self._run([_ENABLED_SH], returncode=1, stderr=stderr)
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].type, 'promptOrNotify')
    self.assertIn('tools/perf/core/bot_platforms.py', results[0].message)

  def testErrorsWhenScriptFailsUnexpectedly(self):
    results, _ = self._run([_ENABLED_SH], returncode=1)
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].type, 'error')
    self.assertIn('exit 1', results[0].message)


if __name__ == '__main__':
  unittest.main()
