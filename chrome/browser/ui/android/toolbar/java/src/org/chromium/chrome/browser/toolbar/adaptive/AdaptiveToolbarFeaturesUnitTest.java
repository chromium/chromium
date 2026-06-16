// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
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
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Unit tests for {@link AdaptiveToolbarFeatures}. */
@Config(manifest = Config.NONE)
@RunWith(BaseRobolectricTestRunner.class)
public class AdaptiveToolbarFeaturesUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;
    @Mock private PrefService mPrefService;

    private Context mContext;

    @Before
    public void setUp() {
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        UserPrefs.setPrefServiceForTesting(mPrefService);
        mContext = ApplicationProvider.getApplicationContext();
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.GLIC)
    @DisableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR,
        ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL
    })
    public void testGetDefaultButtonVariant_BottomBarDisabled_GlicEnabled() {
        assertEquals(
                AdaptiveToolbarButtonVariant.NEW_TAB,
                AdaptiveToolbarFeatures.getDefaultButtonVariant(mContext, mProfile));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.GLIC, ChromeFeatureList.ANDROID_BOTTOM_BAR})
    @DisableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testGetDefaultButtonVariant_BottomBarEnabled_GlicEnabled() {
        assertEquals(
                AdaptiveToolbarButtonVariant.SHARE,
                AdaptiveToolbarFeatures.getDefaultButtonVariant(mContext, mProfile));
    }

    @Test
    @SmallTest
    public void testIsTranslateEnabled_ManagedAndDisabled() {
        when(mPrefService.isManagedPreference(Pref.OFFER_TRANSLATE_ENABLED)).thenReturn(true);
        when(mPrefService.getBoolean(Pref.OFFER_TRANSLATE_ENABLED)).thenReturn(false);
        assertFalse(AdaptiveToolbarFeatures.isTranslateEnabled(mProfile));
    }

    @Test
    @SmallTest
    public void testIsTranslateEnabled_UserModifiedAndDisabled() {
        when(mPrefService.isManagedPreference(Pref.OFFER_TRANSLATE_ENABLED)).thenReturn(false);
        when(mPrefService.getBoolean(Pref.OFFER_TRANSLATE_ENABLED)).thenReturn(false);
        assertTrue(AdaptiveToolbarFeatures.isTranslateEnabled(mProfile));
    }
}
