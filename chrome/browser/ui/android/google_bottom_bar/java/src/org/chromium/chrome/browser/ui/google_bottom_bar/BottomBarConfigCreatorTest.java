// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId.PIH_BASIC;

import android.app.PendingIntent;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;

import java.util.List;

/** Unit tests for {@link BottomBarConfig}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class BottomBarConfigCreatorTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private CustomButtonParams mCustomButtonParams;

    @Test
    public void emptyString_throwsException() {
        assertThrows(
                IllegalArgumentException.class, () -> BottomBarConfigCreator.create("", List.of()));
    }

    @Test
    public void noSpotlightParamList_nullSpotlight_CorrectButtonList() {
        BottomBarConfig buttonConfig = BottomBarConfigCreator.create("0,1,2,3,5", List.of());

        assertNull(buttonConfig.getSpotlightId());
        assertEquals(4, buttonConfig.getButtonList().size());
        assertEquals(PIH_BASIC, buttonConfig.getButtonList().get(0).getId());
    }

    @Test
    public void withSpotlightParamList_correctSpotlightSet_correctButtonList() {
        BottomBarConfig buttonConfig = BottomBarConfigCreator.create("1,1,2,3,5", List.of());
        Integer spotlight = buttonConfig.getSpotlightId();

        assertNotNull(spotlight);
        assertEquals(spotlight.intValue(), PIH_BASIC);
        assertEquals(4, buttonConfig.getButtonList().size());
    }

    @Test
    public void invalidButtonId_throwsException() {
        assertThrows(
                IllegalArgumentException.class,
                () -> BottomBarConfigCreator.create("0,10,1", List.of()));
    }

    @Test
    public void withCorrectCustomParams_hasCorrectButtonConfig() {
        when(mCustomButtonParams.getId()).thenReturn(100); // SAVE
        var pendingIntent = mock(PendingIntent.class);
        when(mCustomButtonParams.getPendingIntent()).thenReturn(pendingIntent);
        // PIH_BASIC, SHARE, SAVE, REFRESH
        BottomBarConfig buttonConfig =
                BottomBarConfigCreator.create("1,1,2,3,5", List.of(mCustomButtonParams));

        // the button has the expected custom button params set
        assertNotNull(buttonConfig.getButtonList().get(2).getButtonParams());
        assertEquals(
                pendingIntent,
                buttonConfig.getButtonList().get(2).getButtonParams().getPendingIntent());
    }
}
