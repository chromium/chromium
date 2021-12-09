// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.content.Context;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.night_mode.AutoDarkFeedbackSourceUnitTest.ShadowWebContentsDarkModeController;
import org.chromium.chrome.browser.night_mode.WebContentsDarkModeController.AutoDarkModeEnabledState;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;

/** Unit test for {@link AutoDarkFeedbackSource}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = ShadowWebContentsDarkModeController.class)
@Features.EnableFeatures(ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
public class AutoDarkFeedbackSourceUnitTest {
    @Implements(WebContentsDarkModeController.class)
    static class ShadowWebContentsDarkModeController {
        static @AutoDarkModeEnabledState int sEnabledState;

        @Implementation
        public static @AutoDarkModeEnabledState int getEnabledState(
                BrowserContextHandle browserContextHandle, Context context, GURL url) {
            return sEnabledState;
        }
    }

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    Profile mProfile;
    @Mock
    Context mContext;

    @Before
    public void setup() {
        ShadowWebContentsDarkModeController.sEnabledState = AutoDarkModeEnabledState.INVALID;
    }

    @After
    public void tearDown() {
        ShadowWebContentsDarkModeController.sEnabledState = -1;
    }

    @Test
    public void testIncognitoProfile() {
        Mockito.doReturn(true).when(mProfile).isOffTheRecord();
        doTestFeedbackSource("");
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
    public void testDisabled_FeatureNotEnabled() {
        doTestFeedbackSource(AutoDarkFeedbackSource.DISABLED_FEATURE_VALUE);
    }

    @Test
    public void testAutoDarkEnabledState_Enabled() {
        ShadowWebContentsDarkModeController.sEnabledState = AutoDarkModeEnabledState.ENABLED;
        doTestFeedbackSource(AutoDarkFeedbackSource.ENABLED_VALUE);
    }

    @Test
    public void testAutoDarkEnabledState_DisabledGlobalSettings() {
        ShadowWebContentsDarkModeController.sEnabledState =
                AutoDarkModeEnabledState.DISABLED_GLOBAL_SETTINGS;
        doTestFeedbackSource(AutoDarkFeedbackSource.DISABLED_GLOBAL_SETTINGS_VALUE);
    }

    @Test
    public void testAutoDarkEnabledState_DisabledLightMode() {
        ShadowWebContentsDarkModeController.sEnabledState =
                AutoDarkModeEnabledState.DISABLED_LIGHT_MODE;
        doTestFeedbackSource(AutoDarkFeedbackSource.DISABLED_BY_LIGHT_MODE_VALUE);
    }

    @Test
    public void testAutoDarkEnabledState_DisabledUrlSettings() {
        ShadowWebContentsDarkModeController.sEnabledState =
                AutoDarkModeEnabledState.DISABLED_URL_SETTINGS;
        doTestFeedbackSource(AutoDarkFeedbackSource.DISABLED_URL_SETTINGS_VALUE);
    }

    private void doTestFeedbackSource(String expectedPsdValue) {
        AutoDarkFeedbackSource source = new AutoDarkFeedbackSource(mProfile, mContext, null);
        String feedbackPsdValue = source.getFeedback().getOrDefault(
                AutoDarkFeedbackSource.AUTO_DARK_FEEDBACK_KEY, "");
        Assert.assertEquals(
                "Expected PSD value does not match.", feedbackPsdValue, expectedPsdValue);
    }
}
