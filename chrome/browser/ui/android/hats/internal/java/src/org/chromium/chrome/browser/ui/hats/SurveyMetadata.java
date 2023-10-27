// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

/**
 * Helper class that holds the information for certain survey triggerId. Internally, this class
 * reads/ writes information into a {@link SharedPreferences}.
 */
class SurveyMetadata {
    /** Shared preferences name that stored survey metadata. */
    private static final String SHARED_PREF_FILENAME = "pref_survey_meta_data";

    /** Key prefix for the date when the survey prompt is displayed for a certain triggerId. */
    @VisibleForTesting
    static final String KEY_PREFIX_DATE_PROMPT_DISPLAYED = "Chrome.Survey.Date.PromptDisplayed.";

    /** Key prefix for the date when a dice roll is performed for a certain triggerId. */
    @VisibleForTesting
    static final String KEY_PREFIX_DATE_DICE_ROLLED = "Chrome.Survey.Date.DiceRolled.";

    /** Key represent the last date any survey prompt is shown. */
    static final String KEY_LAST_PROMPT_DISPLAYED_DATE =
            "Chrome.Survey.Date.LastPromptDisplayedDate";

    private static final int INVALID_DATE = -1;
    private static Integer sDateForTesting;

    /** Helper class used as a LazyHolder to the shared preference storage. */
    private static class Holder {
        static final Object sLock = new Object();
        static @Nullable Holder sInstance;
        private final SharedPreferences mSharedPreferences;

        private Holder(SharedPreferences sharedPreferences) {
            mSharedPreferences = sharedPreferences;
        }

        private static SharedPreferences getSharedPref() {
            assert sInstance != null;
            return sInstance.mSharedPreferences;
        }

        private static void setIntegerPref(String key, int date) {
            synchronized (sLock) {
                getSharedPref().edit().putInt(key, date).apply();
            }
        }
    }

    /**
     * Initialize the holder in the background thread. This is used in order to load the survey
     * share preference before it is requested.
     */
    static void initializeInBackground() {
        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> {
                    Holder.sInstance =
                            new Holder(
                                    ContextUtils.getApplicationContext()
                                            .getSharedPreferences(
                                                    SHARED_PREF_FILENAME, Context.MODE_PRIVATE));
                });
    }

    /** Initialize the holder on the same thread for testing purposes. */
    static void initializeForTesting(SharedPreferences sharedPref, Integer dateForTesting) {
        var original = Holder.sInstance;
        Holder.sInstance = new Holder(sharedPref);
        sDateForTesting = dateForTesting;
        ResettersForTesting.register(
                () -> {
                    Holder.sInstance = original;
                    sDateForTesting = null;
                });
    }

    private final String mPrefKeyPromptDisplayedDate;
    private final String mPrefKeyDiceRolledDate;
    private final Supplier<Integer> mCurrentDateSupplier;
    private Integer mLastDiceRolledDate;
    private Integer mLastPromptDisplayedDate;

    /**
     * Internal class used by SurveyThrottler presenting survey metadata.
     *
     * @param triggerId TriggerId for a certain survey. See {@link SurveyConfig}.
     * @param encodedDateSupplier The supplier that gives an encoded date.
     */
    SurveyMetadata(String triggerId, @NonNull Supplier<Integer> encodedDateSupplier) {
        mCurrentDateSupplier =
                sDateForTesting == null ? encodedDateSupplier : () -> sDateForTesting;
        mPrefKeyPromptDisplayedDate = KEY_PREFIX_DATE_PROMPT_DISPLAYED + triggerId;
        mPrefKeyDiceRolledDate = KEY_PREFIX_DATE_DICE_ROLLED + triggerId;
    }

    static int getLastPromptDisplayedDateForAnySurvey() {
        return Holder.getSharedPref().getInt(KEY_LAST_PROMPT_DISPLAYED_DATE, INVALID_DATE);
    }

    int getCurrentDate() {
        return mCurrentDateSupplier.get();
    }

    int getLastPromptDisplayedDate() {
        if (mLastPromptDisplayedDate == null) {
            mLastPromptDisplayedDate =
                    Holder.getSharedPref().getInt(mPrefKeyPromptDisplayedDate, INVALID_DATE);
        }
        return mLastPromptDisplayedDate;
    }

    int getSurveyLastDiceRolledDate() {
        if (mLastDiceRolledDate == null) {
            mLastDiceRolledDate =
                    Holder.getSharedPref().getInt(mPrefKeyDiceRolledDate, INVALID_DATE);
        }
        return mLastDiceRolledDate;
    }

    void setDiceRolled() {
        if (mLastDiceRolledDate == getCurrentDate()) return;

        mLastDiceRolledDate = getCurrentDate();
        Holder.setIntegerPref(mPrefKeyDiceRolledDate, mLastDiceRolledDate);
    }

    void setPromptDisplayed() {
        if (mLastPromptDisplayedDate != null && mLastPromptDisplayedDate != INVALID_DATE) return;

        mLastPromptDisplayedDate = getCurrentDate();
        Holder.setIntegerPref(mPrefKeyPromptDisplayedDate, mLastPromptDisplayedDate);
        Holder.setIntegerPref(KEY_LAST_PROMPT_DISPLAYED_DATE, mLastPromptDisplayedDate);
    }
}
