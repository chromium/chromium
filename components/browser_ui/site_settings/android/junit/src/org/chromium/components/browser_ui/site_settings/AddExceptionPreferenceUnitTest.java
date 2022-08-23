// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;

/**
 * Unit tests for {@link AddExceptionPreference}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AddExceptionPreferenceUnitTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;

    private boolean mIsPatternValid;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);

        doAnswer(invocation -> mIsPatternValid)
                .when(mWebsitePreferenceBridgeJniMock)
                .isContentSettingsPatternValid(anyString());
        mIsPatternValid = false;
    }

    @Test
    public void testIsPatternValid() {
        assertTrue("Empty pattern should be valid.",
                AddExceptionPreference.isPatternValid("", SiteSettingsCategory.Type.COOKIES));
        mIsPatternValid = true;
        assertFalse(
                "Pattern is invalid when it has colon and the type is not REQUEST_DESKTOP_SITE.",
                AddExceptionPreference.isPatternValid(
                        "https://www.google.com", SiteSettingsCategory.Type.COOKIES));
        assertTrue("Pattern is valid when it has colon and the type is REQUEST_DESKTOP_SITE.",
                AddExceptionPreference.isPatternValid(
                        "https://www.google.com", SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE));
        assertFalse("Pattern is invalid when it has space.",
                AddExceptionPreference.isPatternValid(
                        "https:// google.com", SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE));
        assertFalse("Pattern is invalid when it starts with dot.",
                AddExceptionPreference.isPatternValid(
                        ".google.com", SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE));
        mIsPatternValid = false;
        assertFalse("Pattern is invalid when its ContentSettingsPattern is invalid.",
                AddExceptionPreference.isPatternValid(
                        "https://", SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE));
    }
}