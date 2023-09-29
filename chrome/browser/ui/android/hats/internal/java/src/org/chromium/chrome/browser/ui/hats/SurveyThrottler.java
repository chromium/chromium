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
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Calendar;
import java.util.Random;

/**
 * Class used to check whether survey can be shown based on metadata. The class is also responsible
 * to collect survey related information, and update the metadata for and triggerId.
 */
public class SurveyThrottler {
    /**
     * Reasons that the user was rejected from being selected for a survey
     * Note: these values cannot change and must match the SurveyFilteringResult enum in enums.xml
     * because they're written to logs.
     */
    @IntDef({FilteringResult.SURVEY_PROMPT_ALREADY_DISPLAYED,
            FilteringResult.FORCE_SURVEY_ON_COMMAND_PRESENT,
            FilteringResult.USER_ALREADY_SAMPLED_TODAY, FilteringResult.MAX_NUMBER_MISSING,
            FilteringResult.ROLLED_NON_ZERO_NUMBER, FilteringResult.USER_SELECTED_FOR_SURVEY,
            FilteringResult.FIRST_TIME_USER, FilteringResult.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface FilteringResult {
        int SURVEY_PROMPT_ALREADY_DISPLAYED = 0;
        int FORCE_SURVEY_ON_COMMAND_PRESENT = 2;
        int USER_ALREADY_SAMPLED_TODAY = 3;
        int MAX_NUMBER_MISSING = 4;
        int ROLLED_NON_ZERO_NUMBER = 5;
        int USER_SELECTED_FOR_SURVEY = 6;
        int FIRST_TIME_USER = 8;
        // Number of entries
        int NUM_ENTRIES = 9;
    }

    private final String mTriggerId;
    private final float mProbability;

    private final String mPrefKeyPromptDisplayed;

    /**
     * @param triggerId The trigger Id for the given survey.
     * @param probability The rate an eligible user is randomly selected for the survey.
     */
    public SurveyThrottler(String triggerId, float probability) {
        mTriggerId = triggerId;
        mProbability = probability;

        mPrefKeyPromptDisplayed =
                ChromePreferenceKeys.CHROME_SURVEY_PROMPT_DISPLAYED_TIMESTAMP.createKey(mTriggerId);
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

        // TODO(wenyufu): Check SurveyConfig.isUserPrompted

        if (hasPromptBeenDisplayed()) {
            recordSurveyFilteringResult(FilteringResult.SURVEY_PROMPT_ALREADY_DISPLAYED);
            return false;
        }

        return isRandomlySelectedForSurvey();
    }

    /** Logs in SharedPreferences that the survey prompt was displayed. */
    public void recordSurveyPromptDisplayed() {
        SharedPreferencesManager preferences = SharedPreferencesManager.getInstance();
        preferences.writeLong(mPrefKeyPromptDisplayed, System.currentTimeMillis());
    }

    /**
     * Rolls a random number to see if the user was eligible for the survey. The user will skip the
     * roll if:
     *  1. User is a first time user
     *  2. User as performed the roll today
     *  3. Max number is not setup correctly
     *
     * @return Whether the user is eligible (i.e. the random number rolled was 0).
     */
    private boolean isRandomlySelectedForSurvey() {
        SharedPreferencesManager preferences = SharedPreferencesManager.getInstance();
        int lastDate = preferences.readInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, -1);
        int today = getDayOfYear();
        if (lastDate == today) {
            recordSurveyFilteringResult(FilteringResult.USER_ALREADY_SAMPLED_TODAY);
            return false;
        }

        if (mProbability <= 0) {
            recordSurveyFilteringResult(FilteringResult.MAX_NUMBER_MISSING);
            return false;
        }

        preferences.writeInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, today);
        if (isSelectedWithByRandom()) {
            recordSurveyFilteringResult(FilteringResult.USER_SELECTED_FOR_SURVEY);
            return true;
        } else {
            recordSurveyFilteringResult(FilteringResult.ROLLED_NON_ZERO_NUMBER);
            return false;
        }
    }

    /** @return If the survey info bar for this survey was logged as seen before. */
    private boolean hasPromptBeenDisplayed() {
        SharedPreferencesManager preferences = SharedPreferencesManager.getInstance();
        // TODO(https://crbug.com/1195928): Get an expiration date from feature flag.
        return preferences.readLong(mPrefKeyPromptDisplayed, -1L) != -1L;
    }

    @VisibleForTesting
    boolean isSelectedWithByRandom() {
        return new Random().nextFloat() <= mProbability;
    }

    /** @return The day of the year for today. */
    @VisibleForTesting
    int getDayOfYear() {
        ThreadUtils.assertOnBackgroundThread();
        return Calendar.getInstance().get(Calendar.DAY_OF_YEAR);
    }

    private static void recordSurveyFilteringResult(@FilteringResult int value) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Survey.SurveyFilteringResults", value, FilteringResult.NUM_ENTRIES);
    }

    /** @return Whether survey is enabled by command line flag. */
    private static boolean isSurveyForceEnabled() {
        return CommandLine.getInstance().hasSwitch(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY);
    }
}
