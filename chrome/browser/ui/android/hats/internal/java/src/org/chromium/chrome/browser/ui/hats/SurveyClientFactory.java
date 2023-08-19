// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.ui.hats.SurveyClient.SurveyUiDelegate;

/**
 * Factory class used to create SurveyClient.
 */
public class SurveyClientFactory {
    private static SurveyClientFactory sInstance;

    private SurveyClientFactory() {}

    /**
     * @return The SurveyClientFactory instance.
     */
    public static SurveyClientFactory getInstance() {
        if (sInstance == null) {
            sInstance = new SurveyClientFactory();
        }
        return sInstance;
    }

    /**
     * Create a new survey client with the given config and ui delegate.
     * @param config {@link SurveyConfig#get(String)}
     * @param uiDelegate Ui delegate responsible to show survey.
     * @return SurveyClient to display the given survey matching the config.
     */
    public SurveyClient createClient(
            @NonNull SurveyConfig config, @NonNull SurveyUiDelegate uiDelegate) {
        return new SurveyClientImpl(config, uiDelegate, SurveyControllerProvider.create());
    }
}
