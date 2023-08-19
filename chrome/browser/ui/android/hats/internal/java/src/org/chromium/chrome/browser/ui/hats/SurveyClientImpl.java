// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import android.content.Context;

import java.util.List;

/**
 * Impl for SurveyClient interface.
 */
// TODO(crbug/1400731): Fill in more implementations.
class SurveyClientImpl implements SurveyClient {
    private final SurveyConfig mConfig;
    private final SurveyUiDelegate mUiDelegate;
    private final SurveyController mController;

    SurveyClientImpl(
            SurveyConfig config, SurveyUiDelegate uiDelegate, SurveyController controller) {
        mConfig = config;
        mUiDelegate = uiDelegate;
        mController = controller;
    }

    @Override
    public void showSurvey(Context context) {
        throw new UnsupportedOperationException("Not implemented");
    }

    @Override
    public void showSurvey(
            Context context, List<String> surveyPsdStringValues, List<Boolean> surveyPsdBitValues) {
        throw new UnsupportedOperationException("Not implemented");
    }

    SurveyController getControllerForTesting() {
        return mController;
    }
}
