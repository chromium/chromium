#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys
import unittest

import PRESUBMIT

sys.path.append(
  os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
from PRESUBMIT_test_mocks import MockChange, MockOutputApi

class MockCannedChecks(object):
  def CheckChangeLintsClean(self, input_api, output_api, source_filter,
                            lint_filters, verbose_level):
    return []


class MockInputApi(object):
  """ Mocked input api for unit testing of presubmit.
  This lets us mock things like file system operations and changed files.
  """
  def __init__(self):
    self.canned_checks = MockCannedChecks()
    self.re = re
    self.os_path = os.path
    self.files = []
    self.is_committing = False

  def AffectedFiles(self):
    return self.files

  def AffectedSourceFiles(self):
    return self.files

  def ReadFile(self, f):
    """ Returns the mock contents of f if they've been defined.
    """
    for api_file in self.files:
        if api_file.LocalPath() == f:
            return api_file.NewContents()


class MockFile(object):
  """Mock file object so that presubmit can act invoke file system operations.
  """
  def __init__(self, local_path, new_contents):
    self._local_path = local_path
    self._new_contents = new_contents
    self._changed_contents = ([(i + 1, l) for i, l in enumerate(new_contents)])

  def ChangedContents(self):
    return self._changed_contents

  def NewContents(self):
    return self._new_contents

  def LocalPath(self):
    return self._local_path

  def AbsoluteLocalPath(self):
    return self._local_path


# Format string used as the contents of a mock sync.proto in order to
# test presubmit parsing of EntitySpecifics definition in that file.
MOCK_PROTOFILE_CONTENTS = ('\n'
  'message EntitySpecifics {\n'
  '  //comment\n'
  '\n'
  '  oneof specifics_variant {\n'
  '    AutofillSpecifics autofill = 123;\n'
  '    AppSpecifics app = 456;\n'
  '    AppSettingSpecifics app_setting = 789;\n'
  '    ExtensionSettingSpecifics extension_setting = 910;\n'
  '    ExperimentsSpecifics experiments = 161496;\n'
  '    //comment\n'
  '  }\n'
  '}\n'
  )


# Format string used as the contents of a mock model_type.cc
# in order to test presubmit parsing of the ModelTypeInfoMap in that file.
MOCK_MODELTYPE_CONTENTS =('\n'
  'const ModelTypeInfo kModelTypeInfoMap[] = {\n'
  '// Some comment \n'
  '{APP_SETTINGS, "APP_SETTING", "app_settings", "App settings",\n'
  'sync_pb::EntitySpecifics::kAppSettingFieldNumber, 13},\n'
  '%s\n'
  '};\n')


class ModelTypeInfoChangeTest(unittest.TestCase):
  """Unit testing class that contains tests for sync/PRESUBMIT.py.
  """
  def test_ValidChangeMultiLine(self):
    results = self._testChange('{APPS, "APP", "apps", "Apps",\n'
     'sync_pb::EntitySpecifics::kAppFieldNumber, 12},')
    self.assertEqual(0, len(results))

  def testValidChangeToleratesPluralization(self):
    results = self._testChange('{APPS, "APP", "apps", "App",\n'
      'sync_pb::EntitySpecifics::kAppFieldNumber, 12},')
    self.assertEqual(0, len(results))

  def testValidChangeGrandfatheredEntry(self):
    results = self._testChange('{PROXY_TABS, "", "", "Tabs", -1, 25},')
    self.assertEqual(0, len(results))

  def testValidChangeDeprecatedEntry(self):
    results = self._testChange('{DEPRECATED_EXPERIMENTS, "EXPERIMENTS",'
      '"experiments", "Experiments",'
      'sync_pb::EntitySpecifics::kExperimentsFieldNumber, 19},')
    self.assertEqual(0, len(results))

  def testInvalidChangeMismatchedNotificationType(self):
    results = self._testChange('{AUTOFILL, "AUTOFILL_WRONG", "autofill",\n'
     '"Autofill",sync_pb::EntitySpecifics::kAutofillFieldNumber, 6},')
    self.assertEqual(1, len(results))
    self.assertTrue('notification type' in results[0].message)

  def testInvalidChangeInconsistentModelType(self):
    results = self._testChange('{AUTOFILL, "AUTOFILL", "autofill",\n'
     '"Autofill Extra",sync_pb::EntitySpecifics::kAutofillFieldNumber, 6},')
    self.assertEqual(1, len(results))
    self.assertTrue('model type string' in results[0].message)

  def testInvalidChangeNotTitleCased(self):
    results = self._testChange('{AUTOFILL, "AUTOFILL", "autofill",\n'
     '"autofill",sync_pb::EntitySpecifics::kAutofillFieldNumber, 6},')
    self.assertEqual(1, len(results))
    self.assertTrue('title' in results[0].message)

  def testInvalidChangeInconsistentRootTag(self):
    results = self._testChange('{AUTOFILL, "AUTOFILL", "autofill root",\n'
     '"Autofill",sync_pb::EntitySpecifics::kAutofillFieldNumber, 6},')
    self.assertEqual(1, len(results))
    self.assertTrue('root tag' in results[0].message)

  def testInvalidChangeDuplicatedValues(self):
    results = self._testChange('{APP_SETTINGS, "APP_SETTING",\n'
      '"app_settings", "App settings",\n'
      'sync_pb::EntitySpecifics::kAppSettingFieldNumber, 13},\n')
    self.assertEqual(6, len(results))
    self.assertTrue('APP_SETTINGS' in results[0].message)

  def testBlacklistedRootTag(self):
    results = self._testChange('{EXTENSION_SETTING, "EXTENSION_SETTING",\n'
      '"_mts_schema_descriptor","Extension Setting",\n'
      'sync_pb::EntitySpecifics::kExtensionSettingFieldNumber, 6},')
    self.assertEqual(2, len(results))
    self.assertTrue('_mts_schema_descriptor' in results[0].message)
    self.assertTrue("blacklist" in results[0].message)

  def _testChange(self, modeltype_literal):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile(os.path.abspath('./protocol/sync.proto'),
        MOCK_PROTOFILE_CONTENTS),
      MockFile(os.path.abspath('./base/model_type.cc'),
        MOCK_MODELTYPE_CONTENTS % (modeltype_literal))
    ]

    return PRESUBMIT.CheckChangeOnCommit(mock_input_api, MockOutputApi())


if __name__ == '__main__':
  unittest.main()
