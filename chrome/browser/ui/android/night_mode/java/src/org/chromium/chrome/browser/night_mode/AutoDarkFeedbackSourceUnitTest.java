// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.content.Context;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.AutoDarkFeedbackSourceUnitTest.ShadowWebContentsDarkModeController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;

/** Unit test for {@link AutoDarkFeedbackSource}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = ShadowWebContentsDarkModeController.class)
@EnableFeatures(ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
public class AutoDarkFeedbackSourceUnitTest {
    @Implements(WebContentsDarkModeController.class)
    static class ShadowWebContentsDarkModeController {
        static boolean sEnabledState;

        @Implementation
        public static boolean getEnabledState(
                BrowserContextHandle browserContextHandle, Context context, GURL url) {
            return sEnabledState;
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock Profile mProfile;
    @Mock Context mContext;

    @Before
    public void setup() {
        ShadowWebContentsDarkModeController.sEnabledState = false;
    }

    @After
    public void tearDown() {
        ShadowWebContentsDarkModeController.sEnabledState = false;
    }

    @Test
    public void testIncognitoProfile() {
        Mockito.doReturn(true).when(mProfile).isOffTheRecord();
        doTestFeedbackSource("");
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
    public void testDisabled_FeatureNotEnabled() {
        ShadowWebContentsDarkModeController.sEnabledState = true;
        doTestFeedbackSource(AutoDarkFeedbackSource.DISABLED_VALUE);
    }

    @Test
    public void testAutoDarkEnabledState_Enabled() {
        ShadowWebContentsDarkModeController.sEnabledState = true;
        doTestFeedbackSource(AutoDarkFeedbackSource.ENABLED_VALUE);
    }

    @Test
    public void testAutoDarkEnabledState_DisabledGlobalSettings() {
        ShadowWebContentsDarkModeController.sEnabledState = false;
        doTestFeedbackSource(AutoDarkFeedbackSource.DISABLED_VALUE);
    }

    private void doTestFeedbackSource(String expectedPsdValue) {
        AutoDarkFeedbackSource source = new AutoDarkFeedbackSource(mProfile, mContext, null);
        String feedbackPsdValue =
                source.getFeedback()
                        .getOrDefault(AutoDarkFeedbackSource.AUTO_DARK_FEEDBACK_KEY, "");
        Assert.assertEquals(
                "Expected PSD value does not match.", feedbackPsdValue, expectedPsdValue);
    }
}
