// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
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
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for {@link AdaptiveToolbarFeatures}. */
@Config(manifest = Config.NONE)
@RunWith(BaseRobolectricTestRunner.class)
public class AdaptiveToolbarFeaturesUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorKeyedService;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.GLIC_PROMO_ACCEPTED);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.GLIC)
    public void testShouldForciblyShowGlicButton_FeatureDisabled() {
        assertFalse(AdaptiveToolbarFeatures.shouldForciblyShowGlicButton(mContext, mProfile));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.GLIC)
    public void testShouldForciblyShowGlicButton_NoActiveTask() {
        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(null);
        assertFalse(AdaptiveToolbarFeatures.shouldForciblyShowGlicButton(mContext, mProfile));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.GLIC)
    @DisableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testShouldForciblyShowGlicButton_WithActiveTask() {
        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(mock(ActorTask.class));
        assertTrue(AdaptiveToolbarFeatures.shouldForciblyShowGlicButton(mContext, mProfile));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.GLIC, ChromeFeatureList.ANDROID_BOTTOM_BAR})
    @DisableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testShouldForciblyShowGlicButton_BottomBarEnabled() {
        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(mock(ActorTask.class));
        assertFalse(AdaptiveToolbarFeatures.shouldForciblyShowGlicButton(mContext, mProfile));
    }
}
