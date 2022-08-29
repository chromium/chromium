// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static junit.framework.Assert.assertEquals;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.styles.ChromeColors;

/**
 * Unit tests for {@link OmniboxSuggestionsDropdown}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxSuggestionsDropdownUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private Context mContext;

    private OmniboxSuggestionsDropdown mDropdown;

    @Before
    public void setUp() {
        mContext = new ContextThemeWrapper(
                ApplicationProvider.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    @EnableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE + "<Study",
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_modernize_visual_update_on_tablet/true"})
    public void
    testBackgroundColor_withOmniboxModernizeVisualUpdateFlags() {
        mDropdown = new OmniboxSuggestionsDropdown(mContext);

        assertEquals(mDropdown.getStandardBgColor(),
                ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_1));
        assertEquals(mDropdown.getIncognitoBgColor(),
                mContext.getColor(R.color.default_bg_color_dark_elev_1_baseline));
    }

    @Test
    @SmallTest
    @Feature("Omnibox")
    @DisableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
    public void testBackgroundColor_withoutOmniboxModernizeVisualUpdateFlags() {
        mDropdown = new OmniboxSuggestionsDropdown(mContext);

        assertEquals(
                mDropdown.getStandardBgColor(), ChromeColors.getDefaultThemeColor(mContext, false));
        assertEquals(
                mDropdown.getIncognitoBgColor(), ChromeColors.getDefaultThemeColor(mContext, true));
    }
}