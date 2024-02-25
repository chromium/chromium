// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;

import java.util.Map;

/**
 * Class that controls retrieving and displaying surveys. Clients should call #downloadSurvey() and
 * register a runnable to run when the survey is available. After downloading the survey, call
 * {@link showSurveyIfAvailable()} to display the survey.
 */
public interface SurveyController {
    /**
     * Asynchronously downloads the survey using the provided parameters.
     * @param context The context used to create the survey.
     * @param triggerId  The ID used to fetch the data for the surveys.
     * @param onSuccessRunnable The runnable to notify when the survey is ready.
     * @param onFailureRunnable The runnable to notify when downloading the survey failed, or the
     *                          survey does not exist.
     */
    default void downloadSurvey(
            Context context,
            String triggerId,
            Runnable onSuccessRunnable,
            Runnable onFailureRunnable) {}

    /**
     * Show the survey.
     * @param activity The client activity for the survey request.
     * @param triggerId Id used to trigger the survey.
     * @param displayLogoResId Optional resource id of the logo to be displayed on the survey.
     *                         Pass 0 for no logo.
     * @param lifecycleDispatcher LifecycleDispatcher that will dispatch different activity signals.
     * @param psd key-value set of list of PSD attaching to the survey.
     */
    default void showSurveyIfAvailable(
            Activity activity,
            String triggerId,
            int displayLogoResId,
            @Nullable ActivityLifecycleDispatcher lifecycleDispatcher,
            @Nullable Map<String, String> psd) {}

    /**
     * Check if a survey is valid or expired.
     * @param triggerId  The ID used to fetch the data for the surveys.
     * @return true if the survey has expired, false if the survey is valid.
     */
    default boolean isSurveyExpired(String triggerId) {
        return false;
    }

    /** Destroy the SurveyController and release related resources. */
    default void destroy() {}
}
