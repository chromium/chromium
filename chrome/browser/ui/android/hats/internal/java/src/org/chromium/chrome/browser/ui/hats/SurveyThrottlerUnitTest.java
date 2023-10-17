// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import android.content.SharedPreferences;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.hats.SurveyThrottler.FilteringResult;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Unit tests for {@link SurveyThrottler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SurveyThrottlerUnitTest {
    private static final String TEST_TRIGGER_ID = "foobar";

    private SharedPreferences mSurveyMetadata;

    @Before
    public void setup() {
        mSurveyMetadata = new InMemorySharedPreferences();
        SurveyMetadata.initializeForTesting(mSurveyMetadata, null);

        FirstRunStatus.setFirstRunTriggeredForTesting(false);
        TestThreadUtils.setThreadAssertsDisabled(true);
    }

    @Test
    public void testSuccessfullyShown() {
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(/* randomlySelected= */ true, 1);

        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Survey.SurveyFilteringResults",
                        FilteringResult.USER_SELECTED_FOR_SURVEY)) {
            Assert.assertTrue("Survey should be shown.", throttler.canShowSurvey());
        }
    }

    @Test
    public void testFirstTimeUser() {
        FirstRunStatus.setFirstRunTriggeredForTesting(true);
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(/* randomlySelected= */ true, /* dayOfYear= */ 1);

        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
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
        Assert.assertTrue("User is selected for survey.", throttler1.canShowSurvey());
        throttler1.recordSurveyPromptDisplayed();

        // Try to show the survey in a far enough future.
        int newDateOfYear = dateOfYear + 100;
        RiggedSurveyThrottler throttlerNew =
                new RiggedSurveyThrottler(true, newDateOfYear, triggerId1);
        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Survey.SurveyFilteringResults",
                        FilteringResult.SURVEY_PROMPT_ALREADY_DISPLAYED)) {
            Assert.assertFalse("Survey can't shown if shown before.", throttlerNew.canShowSurvey());
        }
    }

    @Test
    public void testPromptDisplayedForOtherTriggerId() {
        final String triggerId1 = "triggerId1";
        final String triggerId2 = "triggerId2";
        int dateOfYear = 1;

        mSurveyMetadata
                .edit()
                .putLong(
                        SurveyMetadata.KEY_PREFIX_DATE_DICE_ROLLED + triggerId1,
                        System.currentTimeMillis())
                .apply();

        RiggedSurveyThrottler throttler2 =
                new RiggedSurveyThrottler(/* randomlySelected= */ true, dateOfYear, triggerId2);

        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Survey.SurveyFilteringResults",
                        FilteringResult.USER_SELECTED_FOR_SURVEY)) {
            Assert.assertTrue(
                    "Survey with different triggerId can show.", throttler2.canShowSurvey());
        }
    }

    @Test
    public void testPromptRequestedForOtherTriggerId() {
        String triggerId1 = "triggerId1";
        RiggedSurveyThrottler throttler1 =
                new RiggedSurveyThrottler(
                        /* randomlySelected= */ false, /* dayOfYear= */ 1, triggerId1);
        Assert.assertFalse("User is not selected for survey.", throttler1.canShowSurvey());
        Assert.assertEquals(
                "TriggerId should be attempted.", 1, getSurveyLastRequestedDate(triggerId1));

        String triggerId2 = "triggerId2";
        RiggedSurveyThrottler throttler2 =
                new RiggedSurveyThrottler(
                        /* randomlySelected= */ true, /* dayOfYear= */ 2, triggerId2);
        Assert.assertTrue(
                "TriggerId2 is not requested before and should show.", throttler2.canShowSurvey());

        Assert.assertEquals(
                "TriggerId should be attempted.", 2, getSurveyLastRequestedDate(triggerId2));
    }

    @Test
    public void testEligibilityRolledYesterday() {
        setSurveyLastRequestedDate(TEST_TRIGGER_ID, 4);
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(/* randomlySelected= */ true, /* dayOfYear= */ 5);

        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Survey.SurveyFilteringResults",
                        FilteringResult.USER_SELECTED_FOR_SURVEY)) {
            Assert.assertTrue("Random selection should be true", throttler.canShowSurvey());
        }
    }

    @Test
    public void testEligibilityRollingTwiceSameDay() {
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(/* randomlySelected= */ true, /* dayOfYear= */ 5);
        setSurveyLastRequestedDate(TEST_TRIGGER_ID, 5);
        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Survey.SurveyFilteringResults",
                        FilteringResult.USER_ALREADY_SAMPLED_TODAY)) {
            Assert.assertFalse("Random selection should be false.", throttler.canShowSurvey());
        }
    }

    @Test
    public void testEligibilityFirstTimeRollingQualifies() {
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(/* randomlySelected= */ true, /* dayOfYear= */ 5);
        Assert.assertEquals(
                "Last requested date do not exist yet.",
                -1,
                getSurveyLastRequestedDate(TEST_TRIGGER_ID));
        Assert.assertTrue("Random selection should be true", throttler.canShowSurvey());
        Assert.assertEquals("Numbers should match", 5, getSurveyLastRequestedDate(TEST_TRIGGER_ID));
    }

    @Test
    public void testEligibilityFirstTimeRollingDoesNotQualify() {
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(/* randomlySelected= */ false, /* dayOfYear= */ 1);
        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Survey.SurveyFilteringResults",
                        FilteringResult.ROLLED_NON_ZERO_NUMBER)) {
            Assert.assertFalse("Random selection should be false.", throttler.canShowSurvey());
        }
        Assert.assertEquals("Numbers should match", 1, getSurveyLastRequestedDate(TEST_TRIGGER_ID));
    }

    @Test
    public void testUserPromptSurveyAlwaysTriggerWithoutRandomSelection() {
        SurveyConfig config = newSurveyConfig(TEST_TRIGGER_ID, true);
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(
                        /* randomlySelected= */ false, /* dayOfYear= */ 1, config);

        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Survey.SurveyFilteringResults",
                        FilteringResult.USER_PROMPT_SURVEY)) {
            Assert.assertTrue(
                    "User prompted survey will show without random selection.",
                    throttler.canShowSurvey());
        }
    }

    @Test
    @CommandLineFlags.Add(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY)
    public void testCommandLineForceEnableSurvey() {
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(/* randomlySelected= */ false, /* dayOfYear= */ 1);
        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Survey.SurveyFilteringResults",
                        FilteringResult.FORCE_SURVEY_ON_COMMAND_PRESENT)) {
            Assert.assertTrue(
                    "Survey should be enabled by commandline flag.", throttler.canShowSurvey());
        }
    }

    private void setSurveyLastRequestedDate(String triggerId, int value) {
        mSurveyMetadata
                .edit()
                .putInt(SurveyMetadata.KEY_PREFIX_DATE_DICE_ROLLED + triggerId, value)
                .apply();
    }

    private int getSurveyLastRequestedDate(String triggerId) {
        return mSurveyMetadata.getInt(SurveyMetadata.KEY_PREFIX_DATE_DICE_ROLLED + triggerId, -1);
    }

    private static SurveyConfig newSurveyConfig(String triggerId, boolean userPrompted) {
        return new SurveyConfig(
                "trigger", triggerId, 0.5f, userPrompted, new String[0], new String[0]);
    }

    /** Test class used to test the rate limiting logic for {@link SurveyThrottler}. */
    private static class RiggedSurveyThrottler extends SurveyThrottler {
        private final boolean mRandomlySelected;
        private final int mDayOfYear;

        RiggedSurveyThrottler(boolean randomlySelected, int dayOfYear, SurveyConfig config) {
            super(config);

            mRandomlySelected = randomlySelected;
            mDayOfYear = dayOfYear;
        }

        RiggedSurveyThrottler(boolean randomlySelected, int dayOfYear, String triggerId) {
            this(randomlySelected, dayOfYear, newSurveyConfig(triggerId, false));
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
