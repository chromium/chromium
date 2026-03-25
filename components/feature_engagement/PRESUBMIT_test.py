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
  def __init__(self, local_path, new_contents, changed_lines=None):
    self._local_path = local_path
    self._new_contents = new_contents
    self._changed_lines = changed_lines or []

  def LocalPath(self):
    return self._local_path

  def NewContents(self):
    return self._new_contents

  def ChangedContents(self):
    return self._changed_lines

class FeatureEngagementConstantsPresubmitTest(unittest.TestCase):
  FEATURE_CONSTANTS_PATH = (
      'components/feature_engagement/public/android/java/src/org/chromium/'
      'components/feature_engagement/FeatureConstants.java')
  EVENT_CONSTANTS_PATH = (
      'components/feature_engagement/public/android/java/src/org/chromium/'
      'components/feature_engagement/EventConstants.java')

  def testNoAffectedFiles(self):
    input_api = MockInputApi()
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(0, len(results))

  def testSortedFeatureConstants(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(self.FEATURE_CONSTANTS_PATH, [
      '@StringDef({',
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

  def testUnsortedFeatureConstants(self):
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

  def testSortedEventConstants(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(self.EVENT_CONSTANTS_PATH, [
      'public final class EventConstants {',
      '    public static final String A = "a";',
      '    public static final String B = "b";',
      '}'
    ])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(0, len(results))

  def testUnsortedEventConstants(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(self.EVENT_CONSTANTS_PATH, [
      'public final class EventConstants {',
      '    public static final String B = "b";',
      '    public static final String A = "a";',
      '}'
    ])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(1, len(results))
    self.assertEqual('Error', results[0].type)
    self.assertIn('The String constants', results[0].message)
    self.assertIn('Actual item:   B', results[0].message)
    self.assertIn('Expected item: A', results[0].message)

  def testFeatureListSorting(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/public/feature_list.h',
      [],
      [(1, '#if BUILDFLAG(IS_ANDROID)')])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(1, len(results))
    self.assertEqual('Warning', results[0].type)
    self.assertIn('It looks like you are adding a new BUILDFLAG block',
                  results[0].message)

  def testFeatureListSorting_NoBuildflag(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/public/feature_list.h',
      [],
      [(1, 'DEFINE_VARIATION_PARAM(...)')])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(0, len(results))

if __name__ == '__main__':
  unittest.main()
