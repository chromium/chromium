// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import android.app.Activity;

import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;

import java.util.Map;

/** SurveyClient created in charged to show survey. */
public interface SurveyClient {
    /**
     * Show survey in the given activity.
     * @param activity Activity the survey will be showing.
     * @param lifecycleDispatcher LifecycleDispatcher of the given activity.
     */
    void showSurvey(Activity activity, ActivityLifecycleDispatcher lifecycleDispatcher);

    /**
     * Show the survey in the given activity, with the input PSD data.
     *
     * @param activity              Activity the survey will be showing.
     * @param lifecycleDispatcher   LifecycleDispatcher of the given activity.
     * @param surveyPsdBitValues    PSD string values matching the order of {@link
     *                              SurveyConfig#mPsdBitDataFields}.
     * @param surveyPsdStringValues PSD string values matching the order of {@link
     *                              SurveyConfig#mPsdStringDataFields}.
     */
    void showSurvey(
            Activity activity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            Map<String, Boolean> surveyPsdBitValues,
            Map<String, String> surveyPsdStringValues);
}
