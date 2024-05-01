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

import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId.ADD_NOTES;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId.PIH_BASIC;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId.REFRESH;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId.SAVE;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId.SHARE;

import android.app.PendingIntent;
import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link BottomBarConfig}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class BottomBarConfigCreatorTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private CustomButtonParams mCustomButtonParams;

    private BottomBarConfigCreator mConfigCreator;

    @Before
    public void setup() {
        Context context = ContextUtils.getApplicationContext();
        mConfigCreator = new BottomBarConfigCreator(context);
    }

    @Test
    public void emptyString_throwsException() {
        assertThrows(IllegalArgumentException.class, () -> mConfigCreator.create("", List.of()));
    }

    @Test
    public void noSpotlightParamList_nullSpotlight_correctButtonList() {
        BottomBarConfig buttonConfig = mConfigCreator.create("0,1,2,5", List.of());

        assertNull(buttonConfig.getSpotlightId());
        assertEquals(3, buttonConfig.getButtonList().size());
        assertEquals(PIH_BASIC, buttonConfig.getButtonList().get(0).getId());
    }

    @Test
    public void withSpotlightParamList_correctSpotlightSet_correctButtonList() {
        BottomBarConfig buttonConfig = mConfigCreator.create("1,1,2,5", List.of());
        Integer spotlight = buttonConfig.getSpotlightId();

        assertNotNull(spotlight);
        assertEquals(spotlight.intValue(), PIH_BASIC);
        assertEquals(3, buttonConfig.getButtonList().size());
    }

    @Test
    public void createButtonConfigList_emptyCustomButtonParamsList() {
        List<Integer> buttonIdList = List.of(PIH_BASIC, PIH_BASIC, SHARE, SAVE, ADD_NOTES, REFRESH);

        // empty customButtonParamsList - SAVE and ADD_NOTES are not included in the final list
        BottomBarConfig buttonConfig = mConfigCreator.create(buttonIdList, new ArrayList<>());
        assertEquals(3, buttonConfig.getButtonList().size());
    }

    @Test
    public void createButtonConfigList_withCustomButtonParamsList() {
        List<Integer> buttonIdList =
                List.of(
                        PIH_BASIC, PIH_BASIC, SHARE, SAVE, ADD_NOTES,
                        REFRESH); // PIH_BASIC, SHARE, SAVE, ADD_NOTES, REFRESH

        when(mCustomButtonParams.getId()).thenReturn(100); // SAVE

        // ADD_NOTES is not included in the final list
        BottomBarConfig buttonConfig =
                mConfigCreator.create(buttonIdList, List.of(mCustomButtonParams));
        assertEquals(4, buttonConfig.getButtonList().size());
    }

    @Test
    public void createButtonConfigList_buttonIdListWithoutCustomParamId() {
        List<Integer> buttonIdList =
                List.of(PIH_BASIC, PIH_BASIC, SHARE, REFRESH); // PIH_BASIC, SHARE, REFRESH

        when(mCustomButtonParams.getId()).thenReturn(100); // SAVE

        // SAVE is not included in the final list
        BottomBarConfig buttonConfig =
                mConfigCreator.create(buttonIdList, List.of(mCustomButtonParams));
        assertEquals(3, buttonConfig.getButtonList().size());
    }

    @Test
    public void invalidButtonId_throwsException() {
        assertThrows(
                IllegalArgumentException.class, () -> mConfigCreator.create("0,10,1", List.of()));
    }

    @Test
    public void withCorrectCustomParams_hasCorrectButtonConfig() {
        when(mCustomButtonParams.getId()).thenReturn(100); // SAVE
        var pendingIntent = mock(PendingIntent.class);
        when(mCustomButtonParams.getPendingIntent()).thenReturn(pendingIntent);

        // PIH_BASIC, SHARE, SAVE, REFRESH
        BottomBarConfig buttonConfig =
                mConfigCreator.create("1,1,2,3,5", List.of(mCustomButtonParams));

        // the button has the expected custom button params set
        assertEquals(pendingIntent, buttonConfig.getButtonList().get(2).getPendingIntent());
    }
}
