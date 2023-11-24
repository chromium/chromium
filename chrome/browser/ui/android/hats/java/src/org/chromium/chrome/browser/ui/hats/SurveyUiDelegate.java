// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

/**
 * Interface representing the survey invitation UI responsible to show the survey to the user.
 * Client features wanting to customize the survey presentation can override this interface.
 */
public interface SurveyUiDelegate {
    /**
     * Called by SurveyClient when the survey is downloaded and ready to present. When survey
     * is shown, the given runnable(s) are be used to notify SurveyClient the outcome of
     * the survey invitation.
     *
     * @param onSurveyAccepted Callback to run when survey invitation is accepted.
     * @param onSurveyDeclined Callback to run when survey invitation is declined.
     * @param onSurveyPresentationFailed Callback to run when survey invitation failed to show.
     */
    void showSurveyInvitation(
            Runnable onSurveyAccepted,
            Runnable onSurveyDeclined,
            Runnable onSurveyPresentationFailed);

    /**
     * Called by SurveyClient when the survey needs to be dismissed e.g. when
     * survey expires.
     */
    void dismiss();
}
