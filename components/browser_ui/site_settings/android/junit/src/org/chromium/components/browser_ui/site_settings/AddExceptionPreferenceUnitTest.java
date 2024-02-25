// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;

import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;

/** Unit tests for {@link AddExceptionPreference}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AddExceptionPreferenceUnitTest {
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;

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
        assertTrue(
                "Empty pattern should be valid.",
                AddExceptionPreference.isPatternValid(
                        "", SiteSettingsCategory.Type.THIRD_PARTY_COOKIES));
        mIsPatternValid = true;
        assertTrue(
                "Pattern is valid when it has colon and the type is REQUEST_DESKTOP_SITE.",
                AddExceptionPreference.isPatternValid(
                        "https://www.google.com", SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE));
        assertFalse(
                "Pattern is invalid when it has space.",
                AddExceptionPreference.isPatternValid(
                        "https:// google.com", SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE));
        assertFalse(
                "Pattern is invalid when it starts with dot.",
                AddExceptionPreference.isPatternValid(
                        ".google.com", SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE));
        mIsPatternValid = false;
        assertFalse(
                "Pattern is invalid when its ContentSettingsPattern is invalid.",
                AddExceptionPreference.isPatternValid(
                        "https://", SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE));
    }

    @Test
    public void testUpdatePatternIfNeeded() {
        assertEquals(
                "Pattern should not be updated except for REQUEST_DESKTOP_SITE type.",
                "maps.google.com",
                AddExceptionPreference.updatePatternIfNeeded(
                        "maps.google.com", SiteSettingsCategory.Type.THIRD_PARTY_COOKIES, true));
        assertEquals(
                "Pattern should not be updated except for REQUEST_DESKTOP_SITE type.",
                "maps.google.com",
                AddExceptionPreference.updatePatternIfNeeded(
                        "maps.google.com", SiteSettingsCategory.Type.THIRD_PARTY_COOKIES, false));

        AddExceptionPreference.updatePatternIfNeeded(
                "https://maps.google.com", SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE, true);
        verify(mWebsitePreferenceBridgeJniMock)
                .toDomainWildcardPattern(eq("https://maps.google.com"));

        AddExceptionPreference.updatePatternIfNeeded(
                "https://maps.google.com", SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE, false);
        verify(mWebsitePreferenceBridgeJniMock).toHostOnlyPattern(eq("https://maps.google.com"));
    }

    @Test
    public void testGetPrimaryPattern() {
        assertEquals(
                "Primary pattern should always be the original pattern except for"
                        + " THIRD_PARTY_COOKIES type.",
                "maps.google.com",
                AddExceptionPreference.getPrimaryPattern(
                        "maps.google.com", SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE, true));
        assertEquals(
                "Primary pattern should always be the original pattern except for"
                        + " THIRD_PARTY_COOKIES type.",
                "maps.google.com",
                AddExceptionPreference.getPrimaryPattern(
                        "maps.google.com", SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE, false));
        assertEquals(
                "Primary pattern should be wildcard for THIRD_PARTY_COOKIES.",
                SITE_WILDCARD,
                AddExceptionPreference.getPrimaryPattern(
                        "maps.google.com", SiteSettingsCategory.Type.THIRD_PARTY_COOKIES, true));
    }

    @Test
    public void testGetSecondaryPattern() {
        assertEquals(
                "Secondary pattern should always be wildcard except for THIRD_PARTY_COOKIES type.",
                SITE_WILDCARD,
                AddExceptionPreference.getSecondaryPattern(
                        "maps.google.com", SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE, true));
        assertEquals(
                "Secondary pattern should always be wildcard except for THIRD_PARTY_COOKIES type.",
                SITE_WILDCARD,
                AddExceptionPreference.getSecondaryPattern(
                        "maps.google.com", SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE, false));
        assertEquals(
                "Secondary pattern should be the original pattern for THIRD_PARTY_COOKIES",
                "maps.google.com",
                AddExceptionPreference.getSecondaryPattern(
                        "maps.google.com", SiteSettingsCategory.Type.THIRD_PARTY_COOKIES, true));
    }
}
