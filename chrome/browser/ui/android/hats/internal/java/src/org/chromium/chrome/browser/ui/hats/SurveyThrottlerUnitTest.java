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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.hats.SurveyThrottler.FilteringResult;

import java.util.Calendar;

/** Unit tests for {@link SurveyThrottler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SurveyThrottlerUnitTest {
    private static final int TEST_YEAR = 2023;
    private static final int TEST_MONTH = Calendar.JANUARY;
    private static final String TEST_TRIGGER_ID = "foobar";

    private SharedPreferences mSurveyMetadata;

    @Before
    public void setup() {
        mSurveyMetadata = new InMemorySharedPreferences();
        SurveyMetadata.initializeForTesting(mSurveyMetadata, null);

        FirstRunStatus.setFirstRunTriggeredForTesting(false);
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
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
                new RiggedSurveyThrottler(/* randomlySelected= */ true, /* date= */ 1);

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
        int date = 1;
        RiggedSurveyThrottler throttler1 = new RiggedSurveyThrottler(true, date, triggerId1);
        Assert.assertTrue("User is selected for survey.", throttler1.canShowSurvey());
        throttler1.recordSurveyPromptDisplayed();

        // Try to show the survey in a far enough future.
        int newDate = date + 200;
        RiggedSurveyThrottler throttlerNew = new RiggedSurveyThrottler(true, newDate, triggerId1);
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
        int date = 1;

        mSurveyMetadata
                .edit()
                .putInt(SurveyMetadata.KEY_PREFIX_DATE_DICE_ROLLED + triggerId1, 1)
                .apply();

        RiggedSurveyThrottler throttler2 =
                new RiggedSurveyThrottler(/* randomlySelected= */ true, date, triggerId2);

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
                new RiggedSurveyThrottler(/* randomlySelected= */ false, /* date= */ 1, triggerId1);
        Assert.assertFalse("User is not selected for survey.", throttler1.canShowSurvey());
        Assert.assertEquals(
                "Trigger Id should be attempted.",
                throttler1.getEncodedDate(),
                getSurveyLastRequestedDate(triggerId1));

        String triggerId2 = "triggerId2";
        RiggedSurveyThrottler throttler2 =
                new RiggedSurveyThrottler(/* randomlySelected= */ true, /* date= */ 2, triggerId2);
        Assert.assertTrue(
                "TriggerId2 is not requested before and should show.", throttler2.canShowSurvey());
        Assert.assertEquals(
                "Trigger Id should be attempted.",
                throttler2.getEncodedDate(),
                getSurveyLastRequestedDate(triggerId2));
    }

    @Test
    public void testOtherPromptShownRecently() {
        String triggerId1 = "triggerId1";
        RiggedSurveyThrottler throttler1 =
                new RiggedSurveyThrottler(
                        /* randomlySelected= */ true,
                        /* year= */ 2023,
                        /* month= */ 0, // Calendar.JANUARY
                        /* date= */ 1,
                        newSurveyConfig(triggerId1, false));
        Assert.assertTrue("User is selected for survey.", throttler1.canShowSurvey());
        Assert.assertEquals(
                "Trigger Id should be attempted.",
                throttler1.getEncodedDate(),
                getSurveyLastRequestedDate(triggerId1));
        throttler1.recordSurveyPromptDisplayed();

        String triggerId2 = "triggerId2";
        RiggedSurveyThrottler throttler2 =
                new RiggedSurveyThrottler(
                        /* randomlySelected= */ true,
                        /* year= */ 2023,
                        /* month= */ 0, // Calendar.JANUARY
                        /* date= */ 2,
                        newSurveyConfig(triggerId2, false));
        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Survey.SurveyFilteringResults",
                        FilteringResult.OTHER_SURVEY_DISPLAYED_RECENTLY)) {
            Assert.assertFalse(
                    "Survey can't show since other survey is shown recently.",
                    throttler2.canShowSurvey());
        }
    }

    @Test
    public void testOtherPromptShownBeyondRequiredWindow() {
        String triggerId1 = "triggerId1";
        RiggedSurveyThrottler throttler1 =
                new RiggedSurveyThrottler(
                        /* randomlySelected= */ true,
                        /* year= */ 2023,
                        /* month= */ 0, // Calendar.JANUARY
                        /* date= */ 1,
                        newSurveyConfig(triggerId1, false));
        Assert.assertTrue("User is selected for survey.", throttler1.canShowSurvey());
        Assert.assertEquals(
                "TriggerId should be attempted.",
                throttler1.getEncodedDate(),
                getSurveyLastRequestedDate(triggerId1));

        // Required window recorded in SurveyThrottler.MIN_DAYS_BETWEEN_ANY_PROMPT_DISPLAYED = 180
        String triggerId2 = "triggerId2";
        RiggedSurveyThrottler throttler2 =
                new RiggedSurveyThrottler(
                        /* randomlySelected= */ true,
                        /* year= */ 2023,
                        /* month= */ 6, // Calendar.JULY
                        /* date= */ 1,
                        newSurveyConfig(triggerId2, false));
        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Survey.SurveyFilteringResults",
                        FilteringResult.USER_SELECTED_FOR_SURVEY)) {
            Assert.assertTrue(
                    "Survey can show since other survey shown past the required 180 days.",
                    throttler2.canShowSurvey());
        }
        Assert.assertEquals(
                "TriggerId should be attempted.",
                throttler2.getEncodedDate(),
                getSurveyLastRequestedDate(triggerId2));
    }

    @Test
    public void testEligibilityRolledYesterday() {
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(/* randomlySelected= */ true, /* date= */ 5);
        setSurveyLastRequestedDate(TEST_TRIGGER_ID, throttler.getEncodedDate() - 1);

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
                new RiggedSurveyThrottler(/* randomlySelected= */ true, /* date= */ 5);
        setSurveyLastRequestedDate(TEST_TRIGGER_ID, throttler.getEncodedDate());
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
                new RiggedSurveyThrottler(/* randomlySelected= */ true, /* date= */ 1);
        Assert.assertEquals(
                "Last requested date do not exist yet.",
                -1,
                getSurveyLastRequestedDate(TEST_TRIGGER_ID));
        Assert.assertTrue("Random selection should be true", throttler.canShowSurvey());
        Assert.assertEquals(
                "Trigger Id should be attempted.",
                throttler.getEncodedDate(),
                getSurveyLastRequestedDate(TEST_TRIGGER_ID));
    }

    @Test
    public void testEligibilityFirstTimeRollingDoesNotQualify() {
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(/* randomlySelected= */ false, /* date= */ 1);
        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Survey.SurveyFilteringResults",
                        FilteringResult.ROLLED_NON_ZERO_NUMBER)) {
            Assert.assertFalse("Random selection should be false.", throttler.canShowSurvey());
        }
        Assert.assertEquals(
                "Trigger Id should be attempted.",
                throttler.getEncodedDate(),
                getSurveyLastRequestedDate(TEST_TRIGGER_ID));
    }

    @Test
    public void testUserPromptSurveyAlwaysTriggerWithoutRandomSelection() {
        SurveyConfig config = newSurveyConfig(TEST_TRIGGER_ID, true);
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(
                        /* randomlySelected= */ false,
                        /* year= */ 2000,
                        /* month= */ 1,
                        /* date= */ 1,
                        config);
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
                new RiggedSurveyThrottler(/* randomlySelected= */ false, /* date= */ 1);
        try (HistogramWatcher ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Survey.SurveyFilteringResults",
                        FilteringResult.FORCE_SURVEY_ON_COMMAND_PRESENT)) {
            Assert.assertTrue(
                    "Survey should be enabled by commandline flag.", throttler.canShowSurvey());
        }
    }

    @Test
    public void testEncodeDateImpl() {
        Calendar calendar = Calendar.getInstance();

        calendar.set(2020, Calendar.JANUARY, 1);
        int dateJan1st2020 = SurveyThrottler.getEncodedDateImpl(calendar);
        calendar.set(2020, Calendar.FEBRUARY, 1);
        int dateFeb1st2020 = SurveyThrottler.getEncodedDateImpl(calendar);
        Assert.assertEquals(
                "Date in between encoded dates is wrong.", 31, dateFeb1st2020 - dateJan1st2020);

        calendar.set(2019, Calendar.DECEMBER, 31);
        int dateDec31th2019 = SurveyThrottler.getEncodedDateImpl(calendar);
        Assert.assertEquals(
                "The last date has a gap for non-leap year. This is expected.",
                2,
                dateJan1st2020 - dateDec31th2019);

        calendar.set(2020, Calendar.DECEMBER, 31);
        int dateDec31th2020 = SurveyThrottler.getEncodedDateImpl(calendar);
        calendar.set(2021, Calendar.JANUARY, 1);
        int dateJan1st2021 = SurveyThrottler.getEncodedDateImpl(calendar);
        Assert.assertEquals(
                "The last date has no gap for leap year.", 1, dateJan1st2021 - dateDec31th2020);
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
        private final Calendar mCalendar;

        RiggedSurveyThrottler(
                boolean randomlySelected, int year, int month, int date, SurveyConfig config) {
            super(config);

            mRandomlySelected = randomlySelected;
            mCalendar = Calendar.getInstance();
            mCalendar.set(year, month, date);
        }

        RiggedSurveyThrottler(boolean randomlySelected, int date, String triggerId) {
            this(randomlySelected, TEST_YEAR, TEST_MONTH, date, newSurveyConfig(triggerId, false));
        }

        RiggedSurveyThrottler(boolean randomlySelected, int date) {
            this(randomlySelected, date, TEST_TRIGGER_ID);
        }

        @Override
        boolean isSelectedWithByRandom() {
            return mRandomlySelected;
        }

        @Override
        int getEncodedDate() {
            return getEncodedDateImpl(mCalendar);
        }
    }
}
