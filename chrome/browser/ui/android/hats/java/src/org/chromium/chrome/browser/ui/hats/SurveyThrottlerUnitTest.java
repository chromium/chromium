// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.ui.hats.SurveyThrottler.FilteringResult;

/**
 * Unit tests for {@link SurveyThrottler}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SurveyThrottlerUnitTest {
    private static final String TEST_TRIGGER_ID = "foobar";

    private SharedPreferencesManager mSharedPref;
    private ObservableSupplierImpl<Boolean> mCrashUploadEnabledSupplier;

    @Before
    public void setup() {
        mSharedPref = SharedPreferencesManager.getInstance();
        mCrashUploadEnabledSupplier = new ObservableSupplierImpl<>();
        mCrashUploadEnabledSupplier.set(true);

        FirstRunStatus.setFirstRunTriggeredForTesting(false);
    }

    @Test
    public void testSuccessfullyShown() {
        // Set Requirements to show the survey.
        mCrashUploadEnabledSupplier.set(true);
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
        mCrashUploadEnabledSupplier.set(true);
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(/*randomlySelected=*/true, /*dayOfYear=*/1);

        try (HistogramWatcher ignored = HistogramWatcher.newSingleRecordWatcher(
                     "Android.Survey.SurveyFilteringResults", FilteringResult.FIRST_TIME_USER)) {
            Assert.assertFalse(
                    "Survey shouldn't shown for first time users.", throttler.canShowSurvey());
        }
    }

    @Test
    public void testCrashUploadsDisabled() {
        mCrashUploadEnabledSupplier.set(false);
        RiggedSurveyThrottler throttler = new RiggedSurveyThrottler(/*randomlySelected=*/true, 1);

        try (HistogramWatcher ignored =
                        HistogramWatcher.newBuilder()
                                .expectNoRecords("Android.Survey.SurveyFilteringResults")
                                .build()) {
            Assert.assertFalse("Survey shouldn't shown when UMA upload is disabled.",
                    throttler.canShowSurvey());
        }
    }

    @Test
    public void testPromptDisplayedBefore() {
        final String triggerId1 = "triggerId1";
        int dateOfYear = 1;
        RiggedSurveyThrottler throttler1 = new RiggedSurveyThrottler(
                true, dateOfYear, triggerId1, mCrashUploadEnabledSupplier, 0);
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
        mCrashUploadEnabledSupplier.set(true);

        String prefKey1 =
                ChromePreferenceKeys.CHROME_SURVEY_PROMPT_DISPLAYED_TIMESTAMP.createKey(triggerId1);
        mSharedPref.writeLong(prefKey1, System.currentTimeMillis());

        RiggedSurveyThrottler throttler2 = new RiggedSurveyThrottler(
                /*randomlySelected=*/true, dateOfYear, triggerId2, mCrashUploadEnabledSupplier, 0);

        try (HistogramWatcher ignored = HistogramWatcher.newSingleRecordWatcher(
                     "Android.Survey.SurveyFilteringResults",
                     FilteringResult.USER_SELECTED_FOR_SURVEY)) {
            Assert.assertTrue(
                    "Survey with different triggerId can show.", throttler2.canShowSurvey());
        }
    }

    @Test
    public void testEligibilityRolledYesterday() {
        mCrashUploadEnabledSupplier.set(true);

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
        mCrashUploadEnabledSupplier.set(true);
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
        mCrashUploadEnabledSupplier.set(true);
        RiggedSurveyThrottler throttler =
                new RiggedSurveyThrottler(/*randomlySelected=*/true, /*dayOfYear=*/5);
        Assert.assertFalse(mSharedPref.contains(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED));
        Assert.assertTrue("Random selection should be true", throttler.canShowSurvey());
        Assert.assertEquals("Numbers should match", 5,
                mSharedPref.readInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, -1));
    }

    @Test
    public void testDownloadAttemptsWithinCap() {
        mCrashUploadEnabledSupplier.set(true);
        RiggedSurveyThrottler throttler = new RiggedSurveyThrottler(/*randomlySelected=*/true,
                /*dayOfYear=*/1, TEST_TRIGGER_ID, mCrashUploadEnabledSupplier, 1);
        Assert.assertTrue("Download allowed within cap", throttler.canShowSurvey());
    }

    @Test
    public void testStartDownloadIfEligibleTask_DownloadReachCap() {
        mCrashUploadEnabledSupplier.set(true);
        RiggedSurveyThrottler throttler = new RiggedSurveyThrottler(/*randomlySelected=*/true,
                /*dayOfYear=*/1, TEST_TRIGGER_ID, mCrashUploadEnabledSupplier, 1);
        // Assume download attempted previously.
        throttler.recordDownloadAttempted();
        Assert.assertFalse("Download exceed cap.", throttler.canShowSurvey());
    }

    @Test
    public void testEligibilityFirstTimeRollingDoesNotQualify() {
        mCrashUploadEnabledSupplier.set(true);
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

    /** Test class used to test the rate limiting logic for {@link SurveyThrottler}. */
    private class RiggedSurveyThrottler extends SurveyThrottler {
        private final boolean mRandomlySelected;
        private final int mDayOfYear;

        RiggedSurveyThrottler(boolean randomlySelected, int dayOfYear, String triggerId,
                Supplier<Boolean> crashUploadPermissionSupplier, int maxDownloadCap) {
            super(triggerId, 0.5f, crashUploadPermissionSupplier, maxDownloadCap);
            mRandomlySelected = randomlySelected;
            mDayOfYear = dayOfYear;
        }

        RiggedSurveyThrottler(boolean randomlySelected, int dayOfYear) {
            this(randomlySelected, dayOfYear, TEST_TRIGGER_ID, mCrashUploadEnabledSupplier, 0);
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
