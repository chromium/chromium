// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class SurveyClientUnitTest {
    // Test to make sure survey code is executed.
    @Test
    public void smokeTest() {
        SurveyConfig config = new SurveyConfig(
                "trigger", "trigger_id", 0.99, false, new String[0], new String[0]);
        SurveyUiDelegate uiDelegate = new SurveyUiDelegate() {
            @Override
            public void showSurveyInvitation(Runnable onSurveyAccepted, Runnable onSurveyDeclined,
                    Runnable onSurveyPresentationFailed) {}

            @Override
            public void dismiss() {}
        };

        SurveyClient client = SurveyClientFactory.getInstance().createClient(config, uiDelegate);

        if (!(client instanceof SurveyClientImpl)) {
            throw new AssertionError(
                    "SurveyClient is with a different class: " + client.getClass().getName());
        }
        Assert.assertNotNull(
                "Controller is null.", ((SurveyClientImpl) client).getControllerForTesting());
    }
}
