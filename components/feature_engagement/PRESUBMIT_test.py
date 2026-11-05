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


class MockChange(object):
    def __init__(self, input_api):
        self._input_api = input_api
        self.description = ''
        self.issue = None

    def AffectedFiles(self):
        return self._input_api.files

    def DescriptionText(self):
        return self.description


class MockGerrit(object):
    """Minimal stand-in for presubmit_support.GerritAccessor."""

    def __init__(self, owner_email, approvers=None, owners_override=False):
        self._owner_email = owner_email
        self._approvers = list(approvers or [])
        self._owners_override = owners_override

    def IsOwnersOverrideApproved(self, issue):
        return self._owners_override

    def GetChangeOwner(self, issue):
        return self._owner_email

    def GetChangeReviewers(self, issue, approving_only=True):
        return list(self._approvers)


class MockCannedChecks(object):
    """Mirrors presubmit_canned_checks.GetCodereviewOwnerAndReviewers."""

    def GetCodereviewOwnerAndReviewers(
        self, input_api, _email_regexp=None, approval_needed=True
    ):
        issue = input_api.change.issue
        if not issue:
            return None, set()
        owner_email = input_api.gerrit.GetChangeOwner(issue)
        reviewers = set(
            r
            for r in input_api.gerrit.GetChangeReviewers(issue, approval_needed)
            if r != owner_email
        )
        return owner_email, reviewers


class MockInputApi(object):
    def __init__(self):
        self.re = re
        self.files = []
        self.change = MockChange(self)
        self.is_committing = False
        self.dry_run = False
        self.gerrit = None
        self.canned_checks = MockCannedChecks()

    def AffectedFiles(self):
        return [
            f
            for f in self.files
            if f.UnixLocalPath().startswith('components/feature_engagement/')
        ]


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
    def __init__(
        self, local_path, new_contents, changed_lines=None, old_contents=None
    ):
        self._local_path = local_path
        self._new_contents = new_contents
        self._changed_lines = changed_lines or []
        self._old_contents = old_contents or []

    def LocalPath(self):
        return self._local_path

    def UnixLocalPath(self):
        return self._local_path.replace('\\', '/')

    def NewContents(self):
        return self._new_contents

    def ChangedContents(self):
        return self._changed_lines

    def OldContents(self):
        return self._old_contents


class FeatureEngagementConstantsPresubmitTest(unittest.TestCase):
    FEATURE_CONSTANTS_PATH = (
        'components/feature_engagement/public/android/java/src/org/chromium/'
        'components/feature_engagement/FeatureConstants.java'
    )
    EVENT_CONSTANTS_PATH = (
        'components/feature_engagement/public/android/java/src/org/chromium/'
        'components/feature_engagement/EventConstants.java'
    )

    def testNoAffectedFiles(self):
        input_api = MockInputApi()
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testSortedFeatureConstants(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                self.FEATURE_CONSTANTS_PATH,
                [
                    '@StringDef({',
                    '    FeatureConstants.A,',
                    '    FeatureConstants.B,',
                    '})',
                    'public @interface FeatureConstants {',
                    '    String A = "A";',
                    '    String B = "B";',
                    '}',
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testUnsortedFeatureConstants(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                self.FEATURE_CONSTANTS_PATH,
                [
                    '@StringDef({',
                    '    FeatureConstants.A,',
                    '    FeatureConstants.B,',
                    '})',
                    'public @interface FeatureConstants {',
                    '    String B = "B";',
                    '    String A = "A";',
                    '}',
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual('Error', results[0].type)
        self.assertIn('The String constants', results[0].message)

    def testSortedEventConstants(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                self.EVENT_CONSTANTS_PATH,
                [
                    'public final class EventConstants {',
                    '    public static final String A = "a";',
                    '    public static final String B = "b";',
                    '}',
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testUnsortedEventConstants(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                self.EVENT_CONSTANTS_PATH,
                [
                    'public final class EventConstants {',
                    '    public static final String B = "b";',
                    '    public static final String A = "a";',
                    '}',
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual('Error', results[0].type)
        self.assertIn('The String constants', results[0].message)
        self.assertIn('Actual item:   B', results[0].message)
        self.assertIn('Expected item: A', results[0].message)

    def testFeatureListSorting(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_list.h',
                [],
                [(1, '#if BUILDFLAG(IS_ANDROID)')],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual('Warning', results[0].type)
        self.assertIn(
            'It looks like you are adding a new BUILDFLAG block',
            results[0].message,
        )

    def testFeatureListSorting_NoBuildflag(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_list.h',
                [],
                [(1, 'DEFINE_VARIATION_PARAM(...)')],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testNoComparatorAny_ValidCode(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_configurations.cc',
                [],
                [
                    (1, 'config.availability = kAlwaysAvailable;'),
                    (2, 'config.session_rate = kNoRestrictions;'),
                    (3, 'config.session_rate = Comparator(EQUAL, 0);'),
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testNoComparatorAny_ConfigurationHExempt(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/configuration.h',
                [],
                [
                    (1, 'inline constexpr Comparator kAlwaysTrue(ANY, 0);'),
                    (
                        2,
                        'inline constexpr Comparator kAlwaysAvailable(ANY, 0);',
                    ),
                    (3, 'inline constexpr Comparator kNoRestrictions(ANY, 0);'),
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testNoComparatorAny_Violation(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_configurations.cc',
                [],
                [(42, 'config.availability = Comparator(ANY, 0);')],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual('Error', results[0].type)
        self.assertIn(
            'Do not use Comparator(ANY, ...). Use kAlwaysTrue (or kAlwaysAvailable / kNoRestrictions) instead.',
            results[0].message,
        )

    def testNoComparatorAny_ViolationWithNonZero(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_configurations.cc',
                [],
                [(42, 'config.availability = Comparator(ANY, 55);')],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual('Error', results[0].type)
        self.assertIn(
            'Do not use Comparator(ANY, ...). Use kAlwaysTrue (or kAlwaysAvailable / kNoRestrictions) instead.',
            results[0].message,
        )

    def testNoComparatorAny_IgnoredTestFiles(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/configuration_unittest.cc',
                [],
                [(12, 'EXPECT_TRUE(Comparator(ANY, 0).MeetsCriteria(0));')],
            ),
            MockFile(
                'components/feature_engagement/internal/feature_config_condition_validator_unittest.cc',
                [],
                [(25, 'Comparator(ANY, 0);')],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testNoComparatorAny_ViolationInEventTrigger(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_configurations.cc',
                [],
                [
                    (
                        45,
                        'config.trigger = EventConfig('
                        '"iph_feature_trigger", Comparator(ANY, 0), 0, 90);',
                    )
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn(
            'Do not use Comparator(ANY, ...). Use kAlwaysTrue '
            '(or kAlwaysAvailable / kNoRestrictions) instead.',
            errors[0].message,
        )

    def testNoComparatorAny_ViolationInEventUsed(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_configurations.cc',
                [],
                [
                    (
                        48,
                        'config.used = EventConfig('
                        '"iph_feature_used", Comparator(ANY, 0), 0, 90);',
                    )
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn(
            'Do not use Comparator(ANY, ...). Use kAlwaysTrue '
            '(or kAlwaysAvailable / kNoRestrictions) instead.',
            errors[0].message,
        )

    def testNoComparatorAny_ViolationInEventConfigList(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_configurations.cc',
                [],
                [
                    (
                        52,
                        'config.event_configs.insert('
                        'EventConfig("other_event", Comparator(ANY, 0), 0, 30));',
                    )
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual('Error', results[0].type)
        self.assertIn(
            'Do not use Comparator(ANY, ...). Use kAlwaysTrue '
            '(or kAlwaysAvailable / kNoRestrictions) instead.',
            results[0].message,
        )

    def testNoComparatorAny_ValidEventConfigWithAlwaysTrue(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_configurations.cc',
                [],
                [
                    (
                        45,
                        'config.trigger = EventConfig('
                        '"iph_feature_trigger", kAlwaysTrue, 0, 90);',
                    ),
                    (
                        48,
                        'config.used = EventConfig('
                        '"iph_feature_used", kAlwaysTrue, 0, 90);',
                    ),
                    (
                        52,
                        'config.event_configs.insert('
                        'EventConfig("other_event", kAlwaysTrue, 0, 30));',
                    ),
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(0, len(errors))

    def testNoComparatorAny_NonCppFile(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/README.md',
                [],
                [(1, 'Comparator(ANY, 0)')],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testNoRedundantNamespace_ValidCode(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_configurations.cc',
                [],
                [
                    (1, 'config.availability = Comparator(EQUAL, 0);'),
                    (2, 'events::kIOSFREBadgeHoldbackPeriodElapsed'),
                    (3, 'kMaxStoragePeriod'),
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testNoRedundantNamespace_Violation(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_configurations.cc',
                [],
                [
                    (42, 'feature_engagement::events::kChromeOpened'),
                    (43, 'feature_engagement::kMaxStoragePeriod'),
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(2, len(results))
        self.assertEqual('Warning', results[0].type)
        self.assertEqual('Warning', results[1].type)
        self.assertIn(
            'Redundant "feature_engagement::" qualifier', results[0].message
        )
        self.assertIn('ping mschillaci@', results[0].message)

    def testNoRedundantNamespace_IgnoreNamespaceDecl(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/stats.cc',
                [],
                [
                    (9, 'namespace feature_engagement::stats {'),
                    (16, '}  // namespace feature_engagement::stats'),
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testNoRedundantNamespace_IgnoreComments(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/tracker.h',
                [],
                [
                    (14, '// Provides a test feature_engagement::Tracker.'),
                    (15, ' * see feature_engagement::TrackerImpl'),
                    (16, '/* feature_engagement::Tracker */'),
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testNoRedundantNamespace_NonCppFile(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/README.md',
                [],
                [
                    (
                        1,
                        'feature_engagement::TrackerFactory::GetForBrowserContext(profile);',
                    )
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testAlwaysTrueInUsedOrTrigger_ValidRestricted(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_configurations.cc',
                [],
                [
                    (
                        10,
                        'config->used = EventConfig("u", Comparator(EQUAL, 0), '
                        '360, 360);',
                    ),
                    (
                        11,
                        'config->trigger = EventConfig("t", '
                        'Comparator(LESS_THAN, 3), 360, 360);',
                    ),
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
                    (
                        10,
                        'config->event_configs.insert('
                        'EventConfig("other", kAlwaysTrue, 0, 360));',
                    ),
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
                    (
                        10,
                        'config->used = EventConfig("u", kAlwaysTrue, 0, 360);',
                    ),
                    (
                        11,
                        'config->trigger = EventConfig("t", kNoRestrictions, '
                        '0, 360);',
                    ),
                    (
                        12,
                        'EventConfig event_used = EventConfig("u2", '
                        'kAlwaysAvailable, 0, 360);',
                    ),
                    (
                        13,
                        'EventConfig event_trigger = EventConfig("t2", '
                        'kAlwaysTrue, 0, 360);',
                    ),
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
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_configurations.cc',
                [],
                [
                    (
                        10,
                        'config.trigger = EventConfig('
                        '"iph_trigger", kAlwaysTrue, /* window= */ 0, 90);',
                    ),
                    (
                        11,
                        'config.used = EventConfig('
                        '"iph_used", Comparator(EQUAL, 0), 90, 90);',
                    ),
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(0, len(errors))

    def testEventConfigComparatorAndWindow_AlwaysTrueWithNonZeroWindow(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_configurations.cc',
                [],
                [
                    (
                        10,
                        'config.trigger = EventConfig('
                        '"iph_trigger", kAlwaysTrue, 90, 90);',
                    ),
                    (
                        15,
                        'config.used = EventConfig('
                        '"iph_used", kNoRestrictions, 360, 360);',
                    ),
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(2, len(errors))
        self.assertIn('if and only if window is 0', errors[0].message)
        self.assertIn('if and only if window is 0', errors[1].message)

    def testEventConfigComparatorAndWindow_ZeroWindowWithNonAnyComparator(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_configurations.cc',
                [],
                [
                    (
                        20,
                        'config.used = EventConfig('
                        '"iph_used", Comparator(EQUAL, 0), 0, 90);',
                    )
                ],
            )
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual('Error', results[0].type)
        self.assertIn('if and only if window is 0', results[0].message)

    def testNewFlagsHaveMetrics_AllFilesPresent(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_constants.h',
                [],
                [
                    (
                        10,
                        'FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHGoatTeleportationFeature);',
                    )
                ],
            ),
            MockFile(
                'components/feature_engagement/public/feature_constants.cc',
                [],
                [
                    (
                        10,
                        'BASE_FEATURE(kIPHGoatTeleportationFeature, "IPH_GoatTeleportation", '
                        'base::FEATURE_ENABLED_BY_DEFAULT);',
                    )
                ],
            ),
            MockFile(
                'components/feature_engagement/public/feature_list.h',
                [],
                [
                    (
                        10,
                        'DEFINE_VARIATION_PARAM(kIPHGoatTeleportationFeature, '
                        '"IPH_GoatTeleportation");',
                    ),
                    (15, 'VARIATION_ENTRY(kIPHGoatTeleportationFeature)'),
                ],
            ),
            MockFile(
                'components/feature_engagement/public/feature_list.cc',
                [],
                [(10, '    &kIPHGoatTeleportationFeature,')],
            ),
            MockFile('tools/metrics/actions/actions.xml', []),
            MockFile(
                'tools/metrics/histograms/metadata/feature_engagement/histograms.xml',
                [],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(0, len(errors))

    def testNewFlagsHaveMetrics_MissingActionsXml(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_constants.h',
                [],
                [
                    (
                        10,
                        'FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHGoatTeleportationFeature);',
                    )
                ],
            ),
            MockFile(
                'tools/metrics/histograms/metadata/feature_engagement/histograms.xml',
                [],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn('kIPHGoatTeleportationFeature', errors[0].message)
        self.assertIn('tools/metrics/actions/actions.xml', errors[0].message)
        self.assertNotIn(
            'histograms.xml',
            errors[0]
            .message.split('missing from this change:\n')[1]
            .split('\n\n')[0],
        )
        self.assertIn('README.md#required-code-changes', errors[0].message)
        self.assertIn('generate_iph_entry.py', errors[0].message)

    def testNewFlagsHaveMetrics_MissingHistogramsXml(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_constants.cc',
                [],
                [
                    (
                        10,
                        'BASE_FEATURE(kIPHGoatTeleportationFeature, "IPH_GoatTeleportation", '
                        'base::FEATURE_ENABLED_BY_DEFAULT);',
                    )
                ],
            ),
            MockFile('tools/metrics/actions/actions.xml', []),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn('kIPHGoatTeleportationFeature', errors[0].message)
        self.assertIn(
            'tools/metrics/histograms/metadata/feature_engagement/histograms.xml',
            errors[0].message,
        )
        self.assertNotIn(
            'actions.xml',
            errors[0]
            .message.split('missing from this change:\n')[1]
            .split('\n\n')[0],
        )
        self.assertIn('README.md#required-code-changes', errors[0].message)
        self.assertIn('generate_iph_entry.py', errors[0].message)

    def testNewFlagsHaveMetrics_MissingBothXmlFiles(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_list.h',
                [],
                [
                    (
                        10,
                        'DEFINE_VARIATION_PARAM(kIPHGoatTeleportationFeature, "IPH_GoatTeleportation");',
                    )
                ],
            ),
            MockFile(
                'components/feature_engagement/public/feature_list.cc',
                [],
                [(10, '    &kIPHGoatTeleportationFeature,')],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn('kIPHGoatTeleportationFeature', errors[0].message)
        self.assertIn('tools/metrics/actions/actions.xml', errors[0].message)
        self.assertIn(
            'tools/metrics/histograms/metadata/feature_engagement/histograms.xml',
            errors[0].message,
        )
        self.assertIn('README.md#required-code-changes', errors[0].message)
        self.assertIn('generate_iph_entry.py', errors[0].message)

    def testNewFlagsHaveMetrics_NoFlagAdded(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_list.h',
                [],
                [
                    (10, '// Just a comment modification'),
                    (11, '/* Another comment */'),
                ],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(0, len(errors))

    def testNewFlagsHaveMetrics_FeatureConfigurationsCcOnly(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_configurations.cc',
                [],
                [
                    (
                        10,
                        'if (kIPHGoatTeleportationFeature.name == feature->name) {',
                    ),
                    (11, '  return config;'),
                    (12, '}'),
                ],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(0, len(errors))

    def testNewFlagsHaveMetrics_MultiLineFeatureDeclaration(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_constants.h',
                [],
                [
                    (10, 'FEATURE_CONSTANTS_DECLARE_FEATURE('),
                    (11, '    kIPHGoatTeleportationFeature);'),
                ],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn('kIPHGoatTeleportationFeature', errors[0].message)
        self.assertIn('tools/metrics/actions/actions.xml', errors[0].message)
        self.assertIn(
            'tools/metrics/histograms/metadata/feature_engagement/histograms.xml',
            errors[0].message,
        )

    def testNewFlagsHaveMetrics_ExistingFlagReorderedOrMoved(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_constants.h',
                new_contents=[
                    'FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHExistingFeature);',
                ],
                changed_lines=[
                    (
                        20,
                        'FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHExistingFeature);',
                    ),
                ],
                old_contents=[
                    '// Some comment',
                    'FEATURE_CONSTANTS_DECLARE_FEATURE(kIPHExistingFeature);',
                ],
            ),
            MockFile(
                'components/feature_engagement/public/feature_constants.cc',
                new_contents=[
                    'BASE_FEATURE(kIPHExistingFeature, "IPH_Existing", '
                    'base::FEATURE_ENABLED_BY_DEFAULT);',
                ],
                changed_lines=[
                    (
                        25,
                        'BASE_FEATURE(kIPHExistingFeature, "IPH_Existing", '
                        'base::FEATURE_ENABLED_BY_DEFAULT);',
                    ),
                ],
                old_contents=[
                    'BASE_FEATURE(kIPHExistingFeature, "IPH_Existing", '
                    'base::FEATURE_ENABLED_BY_DEFAULT);',
                ],
            ),
            MockFile(
                'components/feature_engagement/public/feature_list.h',
                new_contents=[
                    'DEFINE_VARIATION_PARAM(kIPHExistingFeature, "IPH_Existing");',
                    'VARIATION_ENTRY(kIPHExistingFeature)',
                ],
                changed_lines=[
                    (
                        30,
                        'DEFINE_VARIATION_PARAM(kIPHExistingFeature, "IPH_Existing");',
                    ),
                    (35, 'VARIATION_ENTRY(kIPHExistingFeature)'),
                ],
                old_contents=[
                    'DEFINE_VARIATION_PARAM(kIPHExistingFeature, "IPH_Existing");',
                    'VARIATION_ENTRY(kIPHExistingFeature)',
                ],
            ),
            MockFile(
                'components/feature_engagement/public/feature_list.cc',
                new_contents=[
                    '    &kIPHExistingFeature,',
                ],
                changed_lines=[
                    (40, '    &kIPHExistingFeature,'),
                ],
                old_contents=[
                    '    &kIPHExistingFeature,',
                ],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(0, len(errors))

    def testNewFlagsHaveMetrics_IgnoresNonFeatureListAddressOf(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                'components/feature_engagement/public/feature_list.cc',
                [],
                [
                    (10, '  const auto* ptr = &kSomeOtherVariable;'),
                    (11, '  SomeHelper(&kAnotherVariable);'),
                ],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(0, len(errors))

    def testAllowlistSorting_Sorted(self):
        input_api = MockInputApi()
        input_api.change.description = (
            '[UserEducation.BypassPolicyCheck]=b/571067209 Approved'
        )
        input_api.files = [
            MockFile(
                PRESUBMIT._ENTERPRISE_POLICY_ALLOWLIST_PATH,
                new_contents=[
                    'class EnterprisePolicyBypassAllowlist {',
                    '    static final Set<String> ALLOWLISTED = Set.of(',
                    '        FeatureConstants.A_FEATURE,',
                    '        FeatureConstants.B_FEATURE',
                    '    );',
                    '}',
                ],
                old_contents=[
                    'class EnterprisePolicyBypassAllowlist {',
                    '    static final Set<String> ALLOWLISTED = Set.of(',
                    '        FeatureConstants.A_FEATURE',
                    '    );',
                    '}',
                ],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(0, len(errors))
        warnings = [r for r in results if r.type == 'Warning']
        self.assertEqual(1, len(warnings))
        self.assertNotIn('not sorted alphabetically', warnings[0].message)

    def testAllowlistSorting_UnsortedWarning(self):
        input_api = MockInputApi()
        input_api.change.description = (
            '[UserEducation.BypassPolicyCheck]=b/571067209 Approved'
        )
        input_api.files = [
            MockFile(
                PRESUBMIT._ENTERPRISE_POLICY_ALLOWLIST_PATH,
                new_contents=[
                    'public abstract class EnterprisePolicyBypassAllowlist {',
                    '    static final Set<String> ALLOWLISTED = Set.of(',
                    '        FeatureConstants.B_FEATURE,',
                    '        FeatureConstants.A_FEATURE',
                    '    );',
                    '}',
                ],
                old_contents=[],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        sorting_warnings = [
            r
            for r in results
            if r.type == 'Warning' and 'not sorted alphabetically' in r.message
        ]
        self.assertEqual(1, len(sorting_warnings))

    def testEnterprisePolicyBypass_AllowlistAdditionMissingTag(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                PRESUBMIT._ENTERPRISE_POLICY_ALLOWLIST_PATH,
                new_contents=[
                    '    FeatureConstants.A_FEATURE,',
                    '    FeatureConstants.NEW_EXEMPT_FEATURE',
                ],
                old_contents=[
                    '    FeatureConstants.A_FEATURE',
                ],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn('FeatureConstants.NEW_EXEMPT_FEATURE', errors[0].message)
        self.assertIn('[UserEducation.BypassPolicyCheck]=', errors[0].message)

    def testEnterprisePolicyBypass_InitialCreationWithoutTagAllowed(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                PRESUBMIT._ENTERPRISE_POLICY_ALLOWLIST_PATH,
                new_contents=[
                    'public abstract class EnterprisePolicyBypassAllowlist {',
                    '    static final Set<String> ALLOWLISTED = Set.of(',
                    '        FeatureConstants.IPH_SOME_FEATURE',
                    '    );',
                    '}',
                ],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(0, len(errors))

    def testEnterprisePolicyBypass_InitialCreationUnconditionalReturnTrueStillBlocked(
        self,
    ):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                PRESUBMIT._ENTERPRISE_POLICY_ALLOWLIST_PATH,
                new_contents=[
                    'public abstract class EnterprisePolicyBypassAllowlist {',
                    '    public static boolean '
                    'isAllowedToBypassEnterprisePolicy(String feature) {',
                    '        return true;',
                    '    }',
                    '}',
                ],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn('Do not return true', errors[0].message)

    def testEnterprisePolicyBypass_UnconditionalReturnTrueBlocked(self):
        input_api = MockInputApi()
        input_api.change.description = (
            '[UserEducation.BypassPolicyCheck]=b/571067209 Approved'
        )
        input_api.files = [
            MockFile(
                PRESUBMIT._ENTERPRISE_POLICY_ALLOWLIST_PATH,
                new_contents=[
                    'public abstract class EnterprisePolicyBypassAllowlist {',
                    '    public static boolean '
                    'isAllowedToBypassEnterprisePolicy(String feature) {',
                    '        return true;',
                    '    }',
                    '}',
                ],
                old_contents=[],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn('Do not return true unconditionally', errors[0].message)

    def testEnterprisePolicyBypass_TrackerImplTamperingBlocked(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                PRESUBMIT._TRACKER_IMPL_JAVA_PATH,
                new_contents=[
                    'public class TrackerImpl implements Tracker {',
                    '    // Check removed!',
                    '}',
                ],
                old_contents=[
                    'public class TrackerImpl implements Tracker {',
                    (
                        '    boolean allowed = '
                        'isAllowedToBypassEnterprisePolicy(feature);'
                    ),
                    '}',
                ],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn(
            'Removing the isAllowedToBypassEnterprisePolicy() check',
            errors[0].message,
        )

    def _TaggedAllowlistAddition(self):
        input_api = MockInputApi()
        input_api.change.description = (
            '[UserEducation.BypassPolicyCheck]=b/571067209 Approved'
        )
        input_api.files = [
            MockFile(
                PRESUBMIT._ENTERPRISE_POLICY_ALLOWLIST_PATH,
                new_contents=[
                    '    FeatureConstants.A_FEATURE,',
                    '    FeatureConstants.B_FEATURE',
                ],
                old_contents=['    FeatureConstants.A_FEATURE'],
            ),
        ]
        return input_api

    def testPolicyBypassReview_TaggedUploadWarns(self):
        input_api = self._TaggedAllowlistAddition()
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(0, len(errors))
        warnings = [r for r in results if r.type == 'Warning']
        self.assertEqual(1, len(warnings))
        self.assertIn('Code-Review+1', warnings[0].message)
        self.assertIn('User Education (Java) owner', warnings[0].message)
        self.assertNotIn('jhimawan@google.com', warnings[0].message)
        self.assertNotIn('mschillaci@google.com', warnings[0].message)
        self.assertIn('[UserEducation.BypassPolicyCheck]=', warnings[0].message)

    def testPolicyBypassReview_TagWithoutAllowlistChangeStillWarns(self):
        input_api = MockInputApi()
        input_api.change.description = (
            '[UserEducation.BypassPolicyCheck]=b/571067209 Approved'
        )
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(0, len(errors))
        warnings = [r for r in results if r.type == 'Warning']
        self.assertEqual(1, len(warnings))

    def testPolicyBypassReview_UntaggedAllowlistAdditionNamesOwners(self):
        input_api = self._TaggedAllowlistAddition()
        input_api.change.description = ''
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn('possible owners:', errors[0].message)
        self.assertIn('jhimawan@google.com', errors[0].message)
        self.assertIn('mschillaci@google.com', errors[0].message)
        self.assertIn('[UserEducation.BypassPolicyCheck]=', errors[0].message)
        warnings = [r for r in results if r.type == 'Warning']
        self.assertEqual(0, len(warnings))

    def testPolicyBypassReview_UntaggedTrackerImplTamperingNamesOwners(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                PRESUBMIT._TRACKER_IMPL_JAVA_PATH,
                new_contents=[
                    'public class TrackerImpl implements Tracker {',
                    '    // Check removed!',
                    '}',
                ],
                old_contents=[
                    'public class TrackerImpl implements Tracker {',
                    (
                        '    boolean allowed = '
                        'isAllowedToBypassEnterprisePolicy(feature);'
                    ),
                    '}',
                ],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn('possible owners:', errors[0].message)
        self.assertIn('jhimawan@google.com', errors[0].message)

    def testPolicyBypassReview_DryRunWarnsOnly(self):
        input_api = self._TaggedAllowlistAddition()
        input_api.is_committing = True
        input_api.dry_run = True
        input_api.change.issue = 123
        input_api.gerrit = MockGerrit('dev@chromium.org')
        results = PRESUBMIT.CheckChangeOnCommit(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(0, len(errors))
        warnings = [r for r in results if r.type == 'Warning']
        self.assertEqual(1, len(warnings))
        self.assertNotIn('jhimawan@google.com', warnings[0].message)
        self.assertNotIn('mschillaci@google.com', warnings[0].message)

    def testPolicyBypassReview_CommitWithoutOwnerApprovalBlocked(self):
        input_api = self._TaggedAllowlistAddition()
        input_api.is_committing = True
        input_api.change.issue = 123
        input_api.gerrit = MockGerrit(
            'dev@chromium.org', approvers=['someone@chromium.org']
        )
        results = PRESUBMIT.CheckChangeOnCommit(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn('Code-Review+1', errors[0].message)
        self.assertIn('possible owners:', errors[0].message)
        self.assertIn('jhimawan@google.com', errors[0].message)
        self.assertIn('mschillaci@google.com', errors[0].message)
        warnings = [r for r in results if r.type == 'Warning']
        self.assertEqual(0, len(warnings))

    def testPolicyBypassReview_CommitWithOwnerApprovalPasses(self):
        input_api = self._TaggedAllowlistAddition()
        input_api.is_committing = True
        input_api.change.issue = 123
        input_api.gerrit = MockGerrit(
            'dev@chromium.org', approvers=['mschillaci@google.com']
        )
        results = PRESUBMIT.CheckChangeOnCommit(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testPolicyBypassReview_OwnerAuthoredCommitPasses(self):
        input_api = self._TaggedAllowlistAddition()
        input_api.is_committing = True
        input_api.change.issue = 123
        input_api.gerrit = MockGerrit('jhimawan@google.com')
        results = PRESUBMIT.CheckChangeOnCommit(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testPolicyBypassReview_OwnersOverrideCommitPasses(self):
        input_api = self._TaggedAllowlistAddition()
        input_api.is_committing = True
        input_api.change.issue = 123
        input_api.gerrit = MockGerrit('dev@chromium.org', owners_override=True)
        results = PRESUBMIT.CheckChangeOnCommit(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testPolicyBypassReview_CommitWithoutIssuePasses(self):
        input_api = self._TaggedAllowlistAddition()
        input_api.is_committing = True
        results = PRESUBMIT.CheckChangeOnCommit(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    _UI_FORMAT_OLD = [
        '@IntDef({',
        '    UiFormat.UNKNOWN,',
        '    UiFormat.DIALOG,',
        '})',
        'public @interface UiFormat {',
        '    int UNKNOWN = 0;',
        '    int DIALOG = 10;',
        '}',
    ]
    _UI_FORMAT_NEW = [
        '@IntDef({',
        '    UiFormat.UNKNOWN,',
        '    UiFormat.DIALOG,',
        '    UiFormat.TOAST,',
        '})',
        'public @interface UiFormat {',
        '    int UNKNOWN = 0;',
        '    int DIALOG = 10;',
        '    int TOAST = 11;',
        '}',
    ]

    def _UiFormatAddition(
        self,
        tag=True,
        feature_properties_cases=1,
        tracker_impl_cases=2,
        new_contents=None,
    ):
        input_api = MockInputApi()
        if tag:
            input_api.change.description = (
                '[UserEducation.NewUiFormat]=Toasts are non-interruptive'
            )
        input_api.files = [
            MockFile(
                PRESUBMIT._UI_FORMAT_JAVA_PATH,
                new_contents=new_contents or self._UI_FORMAT_NEW,
                old_contents=self._UI_FORMAT_OLD,
            ),
        ]
        if feature_properties_cases is not None:
            input_api.files.append(
                MockFile(
                    PRESUBMIT._FEATURE_PROPERTIES_JAVA_PATH,
                    new_contents=['case UiFormat.TOAST:']
                    * feature_properties_cases,
                    old_contents=['case UiFormat.DIALOG:'],
                )
            )
        if tracker_impl_cases is not None:
            input_api.files.append(
                MockFile(
                    PRESUBMIT._TRACKER_IMPL_JAVA_PATH,
                    new_contents=['case UiFormat.TOAST:'] * tracker_impl_cases,
                    old_contents=['case UiFormat.DIALOG:'],
                )
            )
        return input_api

    def testUiFormatAdditions_NoNewConstantPasses(self):
        input_api = self._UiFormatAddition(
            tag=False,
            feature_properties_cases=None,
            tracker_impl_cases=None,
            new_contents=self._UI_FORMAT_OLD + ['// trailing comment'],
        )
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testUiFormatAdditions_InitialCreationIgnored(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                PRESUBMIT._UI_FORMAT_JAVA_PATH,
                new_contents=self._UI_FORMAT_NEW,
                old_contents=[],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testUiFormatAdditions_MissingSwitchUpdatesBlocked(self):
        input_api = self._UiFormatAddition(
            feature_properties_cases=None, tracker_impl_cases=None
        )
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn('FeatureProperties.formatToString()', errors[0].message)
        self.assertIn('TrackerImpl.getCooldownFeature()', errors[0].message)
        self.assertIn('UiFormat.TOAST', errors[0].message)

    def testUiFormatAdditions_MissingSecondTrackerImplSwitchBlocked(self):
        input_api = self._UiFormatAddition(tracker_impl_cases=1)
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn('2 case label(s) expected', errors[0].message)
        self.assertIn('found 1', errors[0].message)
        self.assertNotIn(
            'FeatureProperties.formatToString()', errors[0].message
        )

    def testUiFormatAdditions_MissingIntDefEntryBlocked(self):
        new_contents = [
            line for line in self._UI_FORMAT_NEW if line != '    UiFormat.TOAST,'
        ]
        input_api = self._UiFormatAddition(new_contents=new_contents)
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn('missing from the @IntDef list', errors[0].message)

    def testUiFormatAdditions_MissingJustificationTagBlocked(self):
        input_api = self._UiFormatAddition(tag=False)
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn('[UserEducation.NewUiFormat]=', errors[0].message)

    def testUiFormatAdditions_CompleteUpdateWithTagPasses(self):
        input_api = self._UiFormatAddition()
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        self.assertEqual(0, len(results))

    def testNoUnauthorizedTrackerJni_BlocksCustomBridge(self):
        input_api = MockInputApi()
        input_api.files = [
            MockFile(
                (
                    'components/feature_engagement/internal/android/java/src/'
                    'org/chromium/components/feature_engagement/internal/'
                    'CustomBridge.java'
                ),
                new_contents=[
                    '@NativeMethods',
                    'interface Natives {',
                    '    boolean shouldTrigger(long ptr, String feature);',
                    '}',
                ],
                changed_lines=[
                    (1, '@NativeMethods'),
                ],
            ),
        ]
        results = PRESUBMIT.CheckChangeOnUpload(input_api, MockOutputApi())
        errors = [r for r in results if r.type == 'Error']
        self.assertEqual(1, len(errors))
        self.assertIn('Unauthorized JNI bridge', errors[0].message)


if __name__ == '__main__':
    unittest.main()
