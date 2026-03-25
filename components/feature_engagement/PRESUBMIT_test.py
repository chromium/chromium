#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import re
import sys
import os

# To import PRESUBMIT.py from the same directory.
sys.path.append(os.path.dirname(__file__))
import PRESUBMIT

class MockInputApi(object):
  def __init__(self):
    self.re = re
    self.files = []

  def AffectedFiles(self):
    return self.files

class MockOutputApi(object):
  class PresubmitResult(object):
    def __init__(self, message, type):
      self.message = message
      self.type = type

  def PresubmitError(self, message):
    return self.PresubmitResult(message, 'Error')

  def PresubmitPromptWarning(self, message):
    return self.PresubmitResult(message, 'Warning')

class MockFile(object):
  def __init__(self, local_path, new_contents):
    self._local_path = local_path
    self._new_contents = new_contents

  def LocalPath(self):
    return self._local_path

  def NewContents(self):
    return self._new_contents

class FeatureConstantsPresubmitTest(unittest.TestCase):
  FEATURE_CONSTANTS_PATH = (
      'components/feature_engagement/public/android/java/src/org/chromium/'
      'components/feature_engagement/FeatureConstants.java')

  def testNoAffectedFiles(self):
    input_api = MockInputApi()
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(0, len(results))

  def testOtherAffectedFiles(self):
    input_api = MockInputApi()
    input_api.files = [MockFile('other/file.java', ['String A = "A";'])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(0, len(results))

  def testSorted(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(self.FEATURE_CONSTANTS_PATH, [
      '@StringDef({',
      '    // Test comment',
      '    FeatureConstants.A,',
      '    FeatureConstants.B,',
      '})',
      'public @interface FeatureConstants {',
      '    String A = "A";',
      '    String B = "B";',
      '}'
    ])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(0, len(results))

  def testUnsortedStringDef(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(self.FEATURE_CONSTANTS_PATH, [
      '@StringDef({',
      '    FeatureConstants.B,',
      '    FeatureConstants.A,',
      '})',
      'public @interface FeatureConstants {',
      '    String A = "A";',
      '    String B = "B";',
      '}'
    ])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(1, len(results))
    self.assertEqual('Warning', results[0].type)
    self.assertIn('The @StringDef block', results[0].message)
    self.assertIn('Actual item:   FeatureConstants.B', results[0].message)
    self.assertIn('Expected item: FeatureConstants.A', results[0].message)

  def testUnsortedStringConstants(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(self.FEATURE_CONSTANTS_PATH, [
      '@StringDef({',
      '    FeatureConstants.A,',
      '    FeatureConstants.B,',
      '})',
      'public @interface FeatureConstants {',
      '    String B = "B";',
      '    String A = "A";',
      '}'
    ])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(1, len(results))
    self.assertEqual('Error', results[0].type)
    self.assertIn('The String constants', results[0].message)
    self.assertIn('Actual item:   B', results[0].message)
    self.assertIn('Expected item: A', results[0].message)

  def testUnsortedBoth(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(self.FEATURE_CONSTANTS_PATH, [
      '@StringDef({',
      '    FeatureConstants.B,',
      '    FeatureConstants.A,',
      '})',
      'public @interface FeatureConstants {',
      '    String B = "B";',
      '    String A = "A";',
      '}'
    ])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    # It should return both errors/warnings
    self.assertEqual(2, len(results))

if __name__ == '__main__':
  unittest.main()
