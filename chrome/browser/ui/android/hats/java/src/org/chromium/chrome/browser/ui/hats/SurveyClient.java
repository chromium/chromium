// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import android.content.Context;

import java.util.List;

/**
 * SurveyClient created in charged to show survey.
 */
public interface SurveyClient {
    /**
     * Show survey in the given context.
     */
    void showSurvey(Context context);

    /**
     * Show the survey in the given context, with the input PSD data.
     * @param surveyPsdStringValues PSD string values matching the order of {@link
     *         SurveyConfig#mPsdStringDataFields}.
     * @param surveyPsdBitValues PSD string values matching the order of {@link
     *         SurveyConfig#mPsdBitDataFields}.
     */
    void showSurvey(
            Context context, List<String> surveyPsdStringValues, List<Boolean> surveyPsdBitValues);

    /**
     * Interface representing the survey invitation UI responsible to show the survey to the user.
     * Client features wanting to customize the survey presentation can override this interface.
     */
    public interface SurveyUiDelegate {
        /**
         * Called by SurveyClient when the survey is downloaded and ready to present. When survey
         * is shown, the two given runnable can be used to notify SurveyClient the outcome of
         * the survey invitation.
         *
         * @param onSurveyAccepted Callback to run when survey invitation is accepted.
         * @param onSurveyDeclined Callback to run when survey invitation is declined.
         */
        void showSurveyInvitation(Runnable onSurveyAccepted, Runnable onSurveyDeclined);
    }
}
