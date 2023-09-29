// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.ui.hats.SurveyThrottler.FilteringResult;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Unit tests for {@link SurveyThrottler}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SurveyThrottlerUnitTest {
    private static final String TEST_TRIGGER_ID = "foobar";

    private SharedPreferencesManager mSharedPref;

    @Before
    public void setup() {
        mSharedPref = SharedPreferencesManager.getInstance();

        FirstRunStatus.setFirstRunTriggeredForTesting(false);
        TestThreadUtils.setThreadAssertsDisabled(true);
    }

    @Test
    public void testSuccessfullyShown() {
        RiggedSurveyThrottler throttler = new RiggedSurveyThrottler(/*randomlySelected=*/true, 1);

        try (HistogramWatcher ignored = HistogramWatcher.newSingleRecordWatcher(
                     "Android.Survey.SurveyFilteringResults",
                     FilteringResult.USER_SELECTED_FOR_SURVEY)) {
            Assert.assertTrue("Survey should be shown.", throttler.canShowSurvey());
        }
    }

    @Test
    public void testFirstTimeUser() {
        FirstRunStatus.setFirstRunTriggeredForTesting(true);
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(/*randomlySelected=*/true, /*dayOfYear=*/1);

        try (HistogramWatcher ignored = HistogramWatcher.newSingleRecordWatcher(
                     "Android.Survey.SurveyFilteringResults", FilteringResult.FIRST_TIME_USER)) {
            Assert.assertFalse(
                    "Survey shouldn't shown for first time users.", throttler.canShowSurvey());
        }
    }

    @Test
    public void testPromptDisplayedBefore() {
        final String triggerId1 = "triggerId1";
        int dateOfYear = 1;
        RiggedSurveyThrottler throttler1 = new RiggedSurveyThrottler(true, dateOfYear, triggerId1);
        throttler1.recordSurveyPromptDisplayed();
        try (HistogramWatcher ignored = HistogramWatcher.newSingleRecordWatcher(
                     "Android.Survey.SurveyFilteringResults",
                     FilteringResult.SURVEY_PROMPT_ALREADY_DISPLAYED)) {
            Assert.assertFalse("Survey can't shown if shown before.", throttler1.canShowSurvey());
        }
    }

    @Test
    public void testPromptDisplayedForOtherTriggerId() {
        final String triggerId1 = "triggerId1";
        final String triggerId2 = "triggerId2";
        int dateOfYear = 1;

        String prefKey1 =
                ChromePreferenceKeys.CHROME_SURVEY_PROMPT_DISPLAYED_TIMESTAMP.createKey(triggerId1);
        mSharedPref.writeLong(prefKey1, System.currentTimeMillis());

        RiggedSurveyThrottler throttler2 = new RiggedSurveyThrottler(
                /*randomlySelected=*/true, dateOfYear, triggerId2);

        try (HistogramWatcher ignored = HistogramWatcher.newSingleRecordWatcher(
                     "Android.Survey.SurveyFilteringResults",
                     FilteringResult.USER_SELECTED_FOR_SURVEY)) {
            Assert.assertTrue(
                    "Survey with different triggerId can show.", throttler2.canShowSurvey());
        }
    }

    @Test
    public void testEligibilityRolledYesterday() {
        mSharedPref.writeInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, 4);
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(/*randomlySelected=*/true, /*dayOfYear=*/5);

        try (HistogramWatcher ignored = HistogramWatcher.newSingleRecordWatcher(
                     "Android.Survey.SurveyFilteringResults",
                     FilteringResult.USER_SELECTED_FOR_SURVEY)) {
            Assert.assertTrue("Random selection should be true", throttler.canShowSurvey());
        }
    }

    @Test
    public void testEligibilityRollingTwiceSameDay() {
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(/*randomlySelected=*/true, /*dayOfYear=*/5);
        mSharedPref.writeInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, 5);
        try (HistogramWatcher ignored = HistogramWatcher.newSingleRecordWatcher(
                     "Android.Survey.SurveyFilteringResults",
                     FilteringResult.USER_ALREADY_SAMPLED_TODAY)) {
            Assert.assertFalse("Random selection should be false.", throttler.canShowSurvey());
        }
    }

    @Test
    public void testEligibilityFirstTimeRollingQualifies() {
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(/*randomlySelected=*/true, /*dayOfYear=*/5);
        Assert.assertFalse(mSharedPref.contains(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED));
        Assert.assertTrue("Random selection should be true", throttler.canShowSurvey());
        Assert.assertEquals("Numbers should match", 5,
                mSharedPref.readInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, -1));
    }

    @Test
    public void testEligibilityFirstTimeRollingDoesNotQualify() {
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(/*randomlySelected=*/false, /*dayOfYear=*/1);
        Assert.assertFalse(mSharedPref.contains(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED));
        try (HistogramWatcher ignored = HistogramWatcher.newSingleRecordWatcher(
                     "Android.Survey.SurveyFilteringResults",
                     FilteringResult.ROLLED_NON_ZERO_NUMBER)) {
            Assert.assertFalse("Random selection should be false.", throttler.canShowSurvey());
        }
        Assert.assertEquals("Numbers should match", 1,
                mSharedPref.readInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, -1));
    }

    @Test
    @CommandLineFlags.Add(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY)
    public void testCommandLineForceEnableSurvey() {
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(/*randomlySelected=*/false, /*dayOfYear=*/1);
        try (HistogramWatcher ignored = HistogramWatcher.newSingleRecordWatcher(
                     "Android.Survey.SurveyFilteringResults",
                     FilteringResult.FORCE_SURVEY_ON_COMMAND_PRESENT)) {
            Assert.assertTrue(
                    "Survey should be enabled by commandline flag.", throttler.canShowSurvey());
        }
    }

    /** Test class used to test the rate limiting logic for {@link SurveyThrottler}. */
    private class RiggedSurveyThrottler extends SurveyThrottler {
        private final boolean mRandomlySelected;
        private final int mDayOfYear;

        RiggedSurveyThrottler(boolean randomlySelected, int dayOfYear, String triggerId) {
            super(triggerId, 0.5f);
            mRandomlySelected = randomlySelected;
            mDayOfYear = dayOfYear;
        }

        RiggedSurveyThrottler(boolean randomlySelected, int dayOfYear) {
            this(randomlySelected, dayOfYear, TEST_TRIGGER_ID);
        }

        @Override
        boolean isSelectedWithByRandom() {
            return mRandomlySelected;
        }

        @Override
        int getDayOfYear() {
            return mDayOfYear;
        }
    }
}
