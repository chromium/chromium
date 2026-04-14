// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;

/** Unit tests for {@link WebContentsThemeClient}. */
@Config(manifest = Config.NONE)
@RunWith(BaseRobolectricTestRunner.class)
public class WebContentsThemeClientUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Context mContext;
    @Mock private Profile mProfile;
    @Mock private WebContentsDarkModeController.Impl mWebContentsDarkModeControllerImpl;

    private final GURL mGurl = JUnitTestGURLs.URL_1;

    @Before
    public void setup() {
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mContext));
        when(mWebContents.getVisibleUrl()).thenReturn(mGurl);

        Profile.setProfileFromWebContentsForTesting(mProfile);
        WebContentsDarkModeController.setInstanceForTesting(mWebContentsDarkModeControllerImpl);
    }

    @Test
    public void testIsNightModeEnabled_NullWebContents() {
        assertFalse(WebContentsThemeClient.isNightModeEnabled(null));
    }

    @Test
    public void testIsNightModeEnabled_NullWindowAndroid() {
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(null);
        assertFalse(WebContentsThemeClient.isNightModeEnabled(mWebContents));
    }

    @Test
    public void testIsNightModeEnabled_NullContext() {
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(null));
        assertFalse(WebContentsThemeClient.isNightModeEnabled(mWebContents));
    }

    @Test
    public void testIsNightModeEnabled_NightModeOn() {
        ColorUtils.setInNightModeForTesting(true);
        assertTrue(WebContentsThemeClient.isNightModeEnabled(mWebContents));
    }

    @Test
    public void testIsNightModeEnabled_NightModeOff() {
        ColorUtils.setInNightModeForTesting(false);
        assertFalse(WebContentsThemeClient.isNightModeEnabled(mWebContents));
    }

    @Test
    public void testIsForceDarkWebContentEnabled_NullWebContents() {
        assertFalse(WebContentsThemeClient.isForceDarkWebContentEnabled(null));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE)
    public void testIsForceDarkWebContentEnabled_ForceDarkModeEnabled() {
        assertTrue(WebContentsThemeClient.isForceDarkWebContentEnabled(mWebContents));
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE,
        ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING
    })
    public void testIsForceDarkWebContentEnabled_DarkenWebsiteDisabled() {
        assertFalse(WebContentsThemeClient.isForceDarkWebContentEnabled(mWebContents));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE)
    @EnableFeatures(ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
    public void testIsForceDarkWebContentEnabled_NightModeOff() {
        ColorUtils.setInNightModeForTesting(false);
        assertFalse(WebContentsThemeClient.isForceDarkWebContentEnabled(mWebContents));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE)
    @EnableFeatures(ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
    public void testIsForceDarkWebContentEnabled_EnabledForUrl() {
        ColorUtils.setInNightModeForTesting(true);
        when(mWebContentsDarkModeControllerImpl.isEnabledForUrl(eq(mProfile), eq(mGurl)))
                .thenReturn(true);
        assertTrue(WebContentsThemeClient.isForceDarkWebContentEnabled(mWebContents));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.FORCE_WEB_CONTENTS_DARK_MODE)
    @EnableFeatures(ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
    public void testIsForceDarkWebContentEnabled_DisabledForUrl() {
        ColorUtils.setInNightModeForTesting(true);
        when(mWebContentsDarkModeControllerImpl.isEnabledForUrl(eq(mProfile), eq(mGurl)))
                .thenReturn(false);
        assertFalse(WebContentsThemeClient.isForceDarkWebContentEnabled(mWebContents));
    }
}
