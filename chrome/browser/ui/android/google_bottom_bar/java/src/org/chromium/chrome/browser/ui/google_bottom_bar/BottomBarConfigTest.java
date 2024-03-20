// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;

import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.PIH_BASIC;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.REFRESH;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.SAVE;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.SHARE;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link BottomBarConfig}. */
@LooperMode(Mode.PAUSED)
@RunWith(BlockJUnit4ClassRunner.class)
public class BottomBarConfigTest {

    @Test
    public void emptyString_throwsException() {
        assertThrows(IllegalArgumentException.class, () -> BottomBarConfig.fromEncodedString(""));
    }

    @Test
    public void noSpotlightParamList_nullSpotlight_CorrectButtonList() {
        List<Integer> expectedButtons = Arrays.asList(PIH_BASIC, SHARE, SAVE, REFRESH);

        BottomBarConfig buttonConfig = BottomBarConfig.fromEncodedString("0,1,2,3,5");

        assertNull(buttonConfig.getSpotlightId());
        assertEquals(expectedButtons, buttonConfig.getButtonList());
    }

    @Test
    public void withSpotlightParamList_correctSpotlightSet_correctButtonList() {
        List<Integer> expectedButtons = Arrays.asList(PIH_BASIC, SHARE, SAVE, REFRESH);

        BottomBarConfig buttonConfig = BottomBarConfig.fromEncodedString("1,1,2,3,5");
        Integer spotlight = buttonConfig.getSpotlightId();

        assertNotNull(spotlight);
        assertEquals(spotlight.intValue(), PIH_BASIC);
        assertEquals(expectedButtons, buttonConfig.getButtonList());
    }

    @Test
    public void invalidButtonId_throwsException() {
        assertThrows(
                IllegalArgumentException.class, () -> BottomBarConfig.fromEncodedString("0,10,1"));
    }
}
