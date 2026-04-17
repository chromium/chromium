// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link ToolbarVariationUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ToolbarVariationUtilsUnitTest {

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_app_menu_in_toolbar/false",
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_home_button_in_toolbar/false"
    })
    public void testArm1A() {
        // Arm 1A: Back in omnibox, no home, no appmenu
        assertTrue(ToolbarVariationUtils.shouldBackButtonBeInOmnibox());
        assertFalse(ToolbarVariationUtils.shouldAppMenuBeInToolbar());
        assertFalse(ToolbarVariationUtils.shouldHomeButtonBeAtStartOfToolbar());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_app_menu_in_toolbar/true",
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_home_button_in_toolbar/false"
    })
    public void testArm1B() {
        // Arm 1B: Back out of omnibox, no home, yes appmenu
        assertFalse(ToolbarVariationUtils.shouldBackButtonBeInOmnibox());
        assertTrue(ToolbarVariationUtils.shouldAppMenuBeInToolbar());
        assertFalse(ToolbarVariationUtils.shouldHomeButtonBeAtStartOfToolbar());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_app_menu_in_toolbar/true",
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_home_button_in_toolbar/true"
    })
    public void testArm1C() {
        // Arm 1C: Back in omnibox, yes home, yes appmenu
        assertTrue(ToolbarVariationUtils.shouldBackButtonBeInOmnibox());
        assertTrue(ToolbarVariationUtils.shouldAppMenuBeInToolbar());
        assertTrue(ToolbarVariationUtils.shouldHomeButtonBeAtStartOfToolbar());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_app_menu_in_toolbar/true",
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":keep_home_button_in_toolbar/true",
        ChromeFeatureList.ANDROID_BOTTOM_BAR + ":remove_home_button/true"
    })
    public void testArm1C_RemoveHomeButton() {
        assertTrue(ToolbarVariationUtils.shouldBackButtonBeInOmnibox());
        assertTrue(ToolbarVariationUtils.shouldAppMenuBeInToolbar());
        assertFalse(ToolbarVariationUtils.shouldHomeButtonBeAtStartOfToolbar());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsNewToolbarUiEnabled_Enabled() {
        assertTrue(ToolbarVariationUtils.isNewToolbarUiEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsNewToolbarUiEnabled_Disabled() {
        assertFalse(ToolbarVariationUtils.isNewToolbarUiEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/false")
    public void testShouldModifyToolbarButtons_FlagDisabled() {
        // Flag disabled: return true on both.
        assertTrue(ToolbarVariationUtils.shouldModifyToolbarButtons(true));
        assertTrue(ToolbarVariationUtils.shouldModifyToolbarButtons(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/true")
    public void testShouldModifyToolbarButtons_FlagEnabled() {
        // Flag enabled: return false on NTP.
        assertFalse(ToolbarVariationUtils.shouldModifyToolbarButtons(true));
        // Return true on non-NTP.
        assertTrue(ToolbarVariationUtils.shouldModifyToolbarButtons(false));
    }
}
