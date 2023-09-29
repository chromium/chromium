// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.concurrent.ExecutionException;

/**
 * Helper class that C++ can setup the survey testing environment. Java tests should instead use the
 * {@link TestSurveyUtils.TestSurveyComponentRule}.
 */
@JNINamespace("hats")
public class TestSurveyUtilsBridge {
    @CalledByNative
    private static void setupTestSurveyFactory() throws ExecutionException {
        TestSurveyUtils.forceShowSurveyForTesting(true);
        TestSurveyUtils.setUpTestSurveyFactory();
    }

    // Reset methods
    @CalledByNative
    private static void reset() {
        TestSurveyUtils.forceShowSurveyForTesting(false);
        SurveyClientFactory.setInstanceForTesting(null);
    }

    @CalledByNative
    private static String getLastShownTriggerId() {
        SurveyClientFactory instance = SurveyClientFactory.getInstance();

        assert instance instanceof TestSurveyUtils.TestSurveyFactory;
        return ((TestSurveyUtils.TestSurveyFactory) instance).getLastShownTriggerId();
    }
}
