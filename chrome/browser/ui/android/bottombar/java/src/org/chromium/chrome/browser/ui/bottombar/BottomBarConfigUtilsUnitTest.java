// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link BottomBarConfigUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, qualifiers = "sw300dp")
public class BottomBarConfigUtilsUnitTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarEnabled_Tablet() {
        assertFalse(BottomBarConfigUtils.isBottomBarEnabled(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarEnabled_Phone() {
        assertTrue(BottomBarConfigUtils.isBottomBarEnabled(mContext));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsBottomBarDisabled() {
        assertFalse(BottomBarConfigUtils.isBottomBarEnabled(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_home_button_in_toolbar/false")
    public void testShouldIncludeHomeButtonIfEnabled_FalseParam() {
        assertTrue(BottomBarConfigUtils.shouldIncludeHomeButtonIfEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_home_button_in_toolbar/true")
    public void testShouldIncludeHomeButtonIfEnabled_TrueParam() {
        assertFalse(BottomBarConfigUtils.shouldIncludeHomeButtonIfEnabled());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_home_button_in_toolbar/false",
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":remove_home_button/true"
    })
    public void testShouldIncludeHomeButtonIfEnabled_RemoveHomeButtonTrue() {
        assertFalse(BottomBarConfigUtils.shouldIncludeHomeButtonIfEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_app_menu_in_toolbar/false")
    public void testShouldIncludeAppMenuButton_FalseParam() {
        assertTrue(BottomBarConfigUtils.shouldIncludeAppMenuButton());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_app_menu_in_toolbar/true")
    public void testShouldIncludeAppMenuButton_TrueParam() {
        assertFalse(BottomBarConfigUtils.shouldIncludeAppMenuButton());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_bottom_bar_on_gts/true")
    public void testShouldShowOnGts_TrueParam() {
        assertTrue(BottomBarConfigUtils.shouldShowOnGts());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":show_bottom_bar_on_gts/false")
    public void testShouldShowOnGts_FalseParam() {
        assertFalse(BottomBarConfigUtils.shouldShowOnGts());
    }
}
