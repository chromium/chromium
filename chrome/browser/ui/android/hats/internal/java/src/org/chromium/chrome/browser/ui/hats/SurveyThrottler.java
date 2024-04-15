// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.components.browser_ui.util.date.CalendarFactory;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Calendar;
import java.util.Random;

/**
 * Class used to check whether survey can be shown based on metadata. The class is also responsible
 * to collect survey related information, and update the metadata for a triggerId.
 *
 * <p>Internally, the class checks criteria(s) for the given trggerId and global state. If all the
 * throttling checks passed, the instance will perform a dice roll to decide if the survey can show
 * based on the probability.
 */
public class SurveyThrottler {
    private static final int MIN_DAYS_BETWEEN_ANY_PROMPT_DISPLAYED = 180;

    /**
     * Reasons that the user was rejected from being selected for a survey Note: these values cannot
     * change and must match the SurveyFilteringResult enum in enums.xml because they're written to
     * logs.
     */
    @IntDef({
        FilteringResult.SURVEY_PROMPT_ALREADY_DISPLAYED,
        FilteringResult.FORCE_SURVEY_ON_COMMAND_PRESENT,
        FilteringResult.USER_ALREADY_SAMPLED_TODAY,
        FilteringResult.MAX_NUMBER_MISSING,
        FilteringResult.ROLLED_NON_ZERO_NUMBER,
        FilteringResult.USER_SELECTED_FOR_SURVEY,
        FilteringResult.FIRST_TIME_USER,
        FilteringResult.USER_PROMPT_SURVEY,
        FilteringResult.OTHER_SURVEY_DISPLAYED_RECENTLY,
        FilteringResult.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface FilteringResult {
        int SURVEY_PROMPT_ALREADY_DISPLAYED = 0;
        int FORCE_SURVEY_ON_COMMAND_PRESENT = 2;
        int USER_ALREADY_SAMPLED_TODAY = 3;
        int MAX_NUMBER_MISSING = 4;
        int ROLLED_NON_ZERO_NUMBER = 5;
        int USER_SELECTED_FOR_SURVEY = 6;
        int FIRST_TIME_USER = 8;
        int USER_PROMPT_SURVEY = 9;
        int OTHER_SURVEY_DISPLAYED_RECENTLY = 10;

        // Number of entries
        int NUM_ENTRIES = 11;
    }

    private final SurveyConfig mSurveyConfig;
    private SurveyMetadata mMetadata;

    /**
     * @param config Survey config associated with the throttler.
     */
    SurveyThrottler(SurveyConfig config) {
        mSurveyConfig = config;
    }

    /**
     * @return Whether the given survey can be shown.
     */
    public boolean canShowSurvey() {
        // Assert to be run on the background thread, since reading calendar can be a blocking call.
        ThreadUtils.assertOnBackgroundThread();

        if (isSurveyForceEnabled()) {
            recordSurveyFilteringResult(FilteringResult.FORCE_SURVEY_ON_COMMAND_PRESENT);
            return true;
        }

        if (FirstRunStatus.isFirstRunTriggered()) {
            recordSurveyFilteringResult(FilteringResult.FIRST_TIME_USER);
            return false;
        }

        // Ignore the random selection since it's a user prompt survey.
        if (mSurveyConfig.mUserPrompted) {
            recordSurveyFilteringResult(FilteringResult.USER_PROMPT_SURVEY);
            return true;
        }

        return isRandomlySelectedForSurvey();
    }

    /** Logs in SharedPreferences that the survey prompt was displayed. */
    public void recordSurveyPromptDisplayed() {
        getMetadata().setPromptDisplayed();
    }

    /**
     * Rolls a random number to see if the user was eligible for the survey. The user will skip the
     * roll if: 1. User is a first time user; 2. User has performed the roll for the survey today;
     * 3. Max number is not setup correctly.
     *
     * @return Whether the user is eligible (i.e. the random number rolled was 0).
     */
    private boolean isRandomlySelectedForSurvey() {
        int lastDiceRolledDate = getMetadata().getSurveyLastDiceRolledDate();
        int today = getMetadata().getCurrentDate();
        if (lastDiceRolledDate == today) {
            recordSurveyFilteringResult(FilteringResult.USER_ALREADY_SAMPLED_TODAY);
            return false;
        }

        if (mSurveyConfig.mProbability <= 0) {
            recordSurveyFilteringResult(FilteringResult.MAX_NUMBER_MISSING);
            return false;
        }

        // Do not roll when current survey is displayed previously.
        // TODO(crbug.com/40759323): Support configure display survey again by the client.
        if (getMetadata().getLastPromptDisplayedDate() > 0) {
            recordSurveyFilteringResult(FilteringResult.SURVEY_PROMPT_ALREADY_DISPLAYED);
            return false;
        }

        if (today - SurveyMetadata.getLastPromptDisplayedDateForAnySurvey()
                < MIN_DAYS_BETWEEN_ANY_PROMPT_DISPLAYED) {
            recordSurveyFilteringResult(FilteringResult.OTHER_SURVEY_DISPLAYED_RECENTLY);
            return false;
        }

        getMetadata().setDiceRolled();
        if (isSelectedWithByRandom()) {
            recordSurveyFilteringResult(FilteringResult.USER_SELECTED_FOR_SURVEY);
            return true;
        } else {
            recordSurveyFilteringResult(FilteringResult.ROLLED_NON_ZERO_NUMBER);
            return false;
        }
    }

    private SurveyMetadata getMetadata() {
        // Create the metadata lazily since getEncodedDate might be blocking.
        if (mMetadata == null) {
            mMetadata = new SurveyMetadata(mSurveyConfig.mTriggerId, this::getEncodedDate);
        }
        return mMetadata;
    }

    @VisibleForTesting
    boolean isSelectedWithByRandom() {
        return new Random().nextFloat() <= mSurveyConfig.mProbability;
    }

    /**
     * Return the encoded date as int based on the current year and day of year from the calendar.
     */
    @VisibleForTesting
    int getEncodedDate() {
        return getEncodedDateImpl(CalendarFactory.get());
    }

    static int getEncodedDateImpl(Calendar calendar) {
        return calendar.get(Calendar.YEAR) * 366 + calendar.get(Calendar.DAY_OF_YEAR);
    }

    private static void recordSurveyFilteringResult(@FilteringResult int value) {
        // TODO(crbug.com/40283353): Add per-survey metrics.
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Survey.SurveyFilteringResults", value, FilteringResult.NUM_ENTRIES);
    }

    /** @return Whether survey is enabled by command line flag. */
    private static boolean isSurveyForceEnabled() {
        return CommandLine.getInstance().hasSwitch(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY);
    }
}
