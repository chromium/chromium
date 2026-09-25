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

  def testNoComparatorAny_ValidCode(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/public/feature_configurations.cc',
      [],
      [(1, 'config.availability = kAlwaysAvailable;'),
       (2, 'config.session_rate = kNoRestrictions;'),
       (3, 'config.session_rate = Comparator(EQUAL, 0);')])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(0, len(results))

  def testNoComparatorAny_ConfigurationHExempt(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/public/configuration.h',
      [],
      [(1, 'inline constexpr Comparator kAlwaysTrue(ANY, 0);'),
       (2, 'inline constexpr Comparator kAlwaysAvailable(ANY, 0);'),
       (3, 'inline constexpr Comparator kNoRestrictions(ANY, 0);')])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(0, len(results))

  def testNoComparatorAny_Violation(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/public/feature_configurations.cc',
      [],
      [(42, 'config.availability = Comparator(ANY, 0);')])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(1, len(results))
    self.assertEqual('Error', results[0].type)
    self.assertIn('Do not use Comparator(ANY, ...). Use kAlwaysTrue (or kAlwaysAvailable / kNoRestrictions) instead.',
                  results[0].message)

  def testNoComparatorAny_ViolationWithNonZero(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/public/feature_configurations.cc',
      [],
      [(42, 'config.availability = Comparator(ANY, 55);')])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(1, len(results))
    self.assertEqual('Error', results[0].type)
    self.assertIn('Do not use Comparator(ANY, ...). Use kAlwaysTrue (or kAlwaysAvailable / kNoRestrictions) instead.',
                  results[0].message)

  def testNoComparatorAny_IgnoredTestFiles(self):
    input_api = MockInputApi()
    input_api.files = [
      MockFile(
        'components/feature_engagement/public/configuration_unittest.cc',
        [],
        [(12, 'EXPECT_TRUE(Comparator(ANY, 0).MeetsCriteria(0));')]),
      MockFile(
        'components/feature_engagement/internal/feature_config_condition_validator_unittest.cc',
        [],
        [(25, 'Comparator(ANY, 0);')])
    ]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(0, len(results))

  def testNoComparatorAny_ViolationInEventTrigger(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/public/feature_configurations.cc',
      [],
      [(45, 'config.trigger = EventConfig('
            '"iph_feature_trigger", Comparator(ANY, 0), 0, 90);')])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    errors = [r for r in results if r.type == 'Error']
    self.assertEqual(1, len(errors))
    self.assertIn(
        'Do not use Comparator(ANY, ...). Use kAlwaysTrue '
        '(or kAlwaysAvailable / kNoRestrictions) instead.',
        errors[0].message)

  def testNoComparatorAny_ViolationInEventUsed(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/public/feature_configurations.cc',
      [],
      [(48, 'config.used = EventConfig('
            '"iph_feature_used", Comparator(ANY, 0), 0, 90);')])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    errors = [r for r in results if r.type == 'Error']
    self.assertEqual(1, len(errors))
    self.assertIn(
        'Do not use Comparator(ANY, ...). Use kAlwaysTrue '
        '(or kAlwaysAvailable / kNoRestrictions) instead.',
        errors[0].message)

  def testNoComparatorAny_ViolationInEventConfigList(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/public/feature_configurations.cc',
      [],
      [(52, 'config.event_configs.insert('
            'EventConfig("other_event", Comparator(ANY, 0), 0, 30));')])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(1, len(results))
    self.assertEqual('Error', results[0].type)
    self.assertIn(
        'Do not use Comparator(ANY, ...). Use kAlwaysTrue '
        '(or kAlwaysAvailable / kNoRestrictions) instead.',
        results[0].message)

  def testNoComparatorAny_ValidEventConfigWithAlwaysTrue(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/public/feature_configurations.cc',
      [],
      [(45, 'config.trigger = EventConfig('
            '"iph_feature_trigger", kAlwaysTrue, 0, 90);'),
       (48, 'config.used = EventConfig('
            '"iph_feature_used", kAlwaysTrue, 0, 90);'),
       (52, 'config.event_configs.insert('
            'EventConfig("other_event", kAlwaysTrue, 0, 30));')])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    errors = [r for r in results if r.type == 'Error']
    self.assertEqual(0, len(errors))

  def testNoComparatorAny_NonCppFile(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/README.md',
      [],
      [(1, 'Comparator(ANY, 0)')])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(0, len(results))

  def testNoRedundantNamespace_ValidCode(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/public/feature_configurations.cc',
      [],
      [(1, 'config.availability = Comparator(EQUAL, 0);'),
       (2, 'events::kIOSFREBadgeHoldbackPeriodElapsed'),
       (3, 'kMaxStoragePeriod')]) ]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(0, len(results))

  def testNoRedundantNamespace_Violation(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/public/feature_configurations.cc',
      [],
      [(42, 'feature_engagement::events::kChromeOpened'),
       (43, 'feature_engagement::kMaxStoragePeriod')]) ]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(2, len(results))
    self.assertEqual('Warning', results[0].type)
    self.assertEqual('Warning', results[1].type)
    self.assertIn('Redundant "feature_engagement::" qualifier', results[0].message)
    self.assertIn('ping mschillaci@', results[0].message)

  def testNoRedundantNamespace_IgnoreNamespaceDecl(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/public/stats.cc',
      [],
      [(9, 'namespace feature_engagement::stats {'),
       (16, '}  // namespace feature_engagement::stats')]) ]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(0, len(results))

  def testNoRedundantNamespace_IgnoreComments(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/public/tracker.h',
      [],
      [(14, '// Provides a test feature_engagement::Tracker.'),
       (15, ' * see feature_engagement::TrackerImpl'),
       (16, '/* feature_engagement::Tracker */')]) ]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(0, len(results))

  def testNoRedundantNamespace_NonCppFile(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/README.md',
      [],
      [(1, 'feature_engagement::TrackerFactory::GetForBrowserContext(profile);')]) ]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(0, len(results))

  def testAlwaysTrueInUsedOrTrigger_ValidRestricted(self):
    input_api = MockInputApi()
    input_api.files = [
        MockFile(
            'components/feature_engagement/public/feature_configurations.cc',
            [],
            [
                (10, 'config->used = EventConfig("u", Comparator(EQUAL, 0), '
                     '360, 360);'),
                (11, 'config->trigger = EventConfig("t", '
                     'Comparator(LESS_THAN, 3), 360, 360);'),
            ],
        )
    ]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(0, len(results))

  def testAlwaysTrueInUsedOrTrigger_ValidInPreconditions(self):
    input_api = MockInputApi()
    input_api.files = [
        MockFile(
            'components/feature_engagement/public/feature_configurations.cc',
            [],
            [
                (10, 'config->event_configs.insert('
                     'EventConfig("other", kAlwaysTrue, 0, 360));'),
            ],
        )
    ]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(0, len(results))

  def testAlwaysTrueInUsedOrTrigger_WarnsOnUsedAndTrigger(self):
    input_api = MockInputApi()
    input_api.files = [
        MockFile(
            'components/feature_engagement/public/feature_configurations.cc',
            [],
            [
                (10, 'config->used = EventConfig("u", kAlwaysTrue, 0, 360);'),
                (11, 'config->trigger = EventConfig("t", kNoRestrictions, '
                     '0, 360);'),
                (12, 'EventConfig event_used = EventConfig("u2", '
                     'kAlwaysAvailable, 0, 360);'),
                (13, 'EventConfig event_trigger = EventConfig("t2", '
                     'kAlwaysTrue, 0, 360);'),
            ],
        )
    ]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(4, len(results))
    for res in results:
      self.assertEqual('Warning', res.type)
      self.assertIn('passive data recording only', res.message)

  def testEventConfigComparatorAndWindow_Valid(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/public/feature_configurations.cc',
      [],
      [(10, 'config.trigger = EventConfig('
            '"iph_trigger", kAlwaysTrue, /* window= */ 0, 90);'),
       (11, 'config.used = EventConfig('
            '"iph_used", Comparator(EQUAL, 0), 90, 90);')])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    errors = [r for r in results if r.type == 'Error']
    self.assertEqual(0, len(errors))

  def testEventConfigComparatorAndWindow_AlwaysTrueWithNonZeroWindow(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/public/feature_configurations.cc',
      [],
      [(10, 'config.trigger = EventConfig('
            '"iph_trigger", kAlwaysTrue, 90, 90);'),
       (15, 'config.used = EventConfig('
            '"iph_used", kNoRestrictions, 360, 360);')])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    errors = [r for r in results if r.type == 'Error']
    self.assertEqual(2, len(errors))
    self.assertIn('if and only if window is 0', errors[0].message)
    self.assertIn('if and only if window is 0', errors[1].message)

  def testEventConfigComparatorAndWindow_ZeroWindowWithNonAnyComparator(self):
    input_api = MockInputApi()
    input_api.files = [MockFile(
      'components/feature_engagement/public/feature_configurations.cc',
      [],
      [(20, 'config.used = EventConfig('
            '"iph_used", Comparator(EQUAL, 0), 0, 90);')])]
    results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
    self.assertEqual(1, len(results))
    self.assertEqual('Error', results[0].type)
    self.assertIn('if and only if window is 0', results[0].message)

if __name__ == '__main__':
  unittest.main()
