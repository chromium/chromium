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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;

/** Unit test for {@link AutoDarkFeedbackSource}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
public class AutoDarkFeedbackSourceUnitTest {
    private boolean mEnabledState;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock Profile mProfile;
    @Mock Context mContext;

    @Before
    public void setup() {
        WebContentsDarkModeController.setInstanceForTesting(
                new WebContentsDarkModeController.Impl() {
                    @Override
                    public boolean getEnabledState(
                            BrowserContextHandle browserContextHandle, Context context, GURL url) {
                        return mEnabledState;
                    }
                });
        mEnabledState = false;
    }

    @After
    public void tearDown() {
        mEnabledState = false;
    }

    @Test
    public void testIncognitoProfile() {
        Mockito.doReturn(true).when(mProfile).isOffTheRecord();
        doTestFeedbackSource("");
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
    public void testDisabled_FeatureNotEnabled() {
        mEnabledState = true;
        doTestFeedbackSource(AutoDarkFeedbackSource.DISABLED_VALUE);
    }

    @Test
    public void testAutoDarkEnabledState_Enabled() {
        mEnabledState = true;
        doTestFeedbackSource(AutoDarkFeedbackSource.ENABLED_VALUE);
    }

    @Test
    public void testAutoDarkEnabledState_DisabledGlobalSettings() {
        mEnabledState = false;
        doTestFeedbackSource(AutoDarkFeedbackSource.DISABLED_VALUE);
    }

    private void doTestFeedbackSource(String expectedPsdValue) {
        AutoDarkFeedbackSource source = new AutoDarkFeedbackSource(mProfile, mContext, null);
        String feedbackPsdValue =
                source.getFeedback()
                        .getOrDefault(AutoDarkFeedbackSource.AUTO_DARK_FEEDBACK_KEY, "");
        Assert.assertEquals(
                "Expected PSD value does not match.", expectedPsdValue, feedbackPsdValue);
    }
}
