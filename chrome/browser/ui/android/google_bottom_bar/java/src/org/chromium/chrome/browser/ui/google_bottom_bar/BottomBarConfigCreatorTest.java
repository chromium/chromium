// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId.ADD_NOTES;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId.PIH_BASIC;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId.REFRESH;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId.SAVE;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId.SHARE;

import android.app.PendingIntent;
import android.content.Context;
import android.graphics.drawable.Drawable;

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
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfigCreator.ButtonId;

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
    private Context mContext;

    @Before
    public void setup() {
        mContext = ContextUtils.getApplicationContext();
        mConfigCreator = new BottomBarConfigCreator(mContext);
    }

    @Test
    public void emptyString_returnsDefaultConfig() {
        assertDefaultConfig(mConfigCreator.create("", List.of()));
    }

    @Test
    public void onlyOneItem_returnsDefaultConfig() {
        assertDefaultConfig(mConfigCreator.create("1", List.of()));
    }

    @Test
    public void invalidButtonIdInList_returnsDefaultConfig() {
        assertDefaultConfig(mConfigCreator.create("0,10,1", List.of()));
    }

    @Test
    public void invalidSpotlightButton_returnsDefaultConfig() {
        assertDefaultConfig(mConfigCreator.create("10,1,2,3", List.of()));
    }

    @Test
    public void noSpotlightParamList_nullSpotlight_correctButtonList() {
        BottomBarConfig buttonConfig = mConfigCreator.create("0,1,2,3", List.of());

        assertNull(buttonConfig.getSpotlightId());
        assertEquals(3, buttonConfig.getButtonList().size());
        assertEquals(PIH_BASIC, buttonConfig.getButtonList().get(0).getId());
    }

    @Test
    public void withSpotlightParamList_correctSpotlightSet_correctButtonList() {
        BottomBarConfig buttonConfig = mConfigCreator.create("1,1,2,3", List.of());
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
        Drawable drawable = mock(Drawable.class);
        when(mCustomButtonParams.getId()).thenReturn(100); // SAVE
        when(mCustomButtonParams.getIcon(mContext)).thenReturn(drawable);

        // ADD_NOTES and REFRESH are not included in the final list as they are not supported
        BottomBarConfig buttonConfig =
                mConfigCreator.create(buttonIdList, List.of(mCustomButtonParams));
        assertEquals(3, buttonConfig.getButtonList().size());
    }

    @Test
    public void createButtonConfigList_buttonIdListWithoutCustomParamId() {
        List<Integer> buttonIdList = List.of(PIH_BASIC, PIH_BASIC, SHARE); // PIH_BASIC, SHARE

        when(mCustomButtonParams.getId()).thenReturn(100); // SAVE

        // SAVE is not included in the final list
        BottomBarConfig buttonConfig =
                mConfigCreator.create(buttonIdList, List.of(mCustomButtonParams));
        assertEquals(2, buttonConfig.getButtonList().size());
    }

    @Test
    public void withCorrectCustomParams_hasCorrectButtonConfig() {
        Drawable drawable = mock(Drawable.class);
        when(mCustomButtonParams.getId()).thenReturn(100); // SAVE
        when(mCustomButtonParams.getIcon(mContext)).thenReturn(drawable);
        var pendingIntent = mock(PendingIntent.class);
        when(mCustomButtonParams.getPendingIntent()).thenReturn(pendingIntent);

        // PIH_BASIC, SHARE, SAVE, REFRESH
        BottomBarConfig buttonConfig =
                mConfigCreator.create("1,1,2,3,5", List.of(mCustomButtonParams));

        // the button has the expected custom button params set
        assertEquals(pendingIntent, buttonConfig.getButtonList().get(2).getPendingIntent());
    }

    private static void assertDefaultConfig(BottomBarConfig config) {
        assertNull(config.getSpotlightId());
        assertEquals(3, config.getButtonList().size());
        assertEquals(ButtonId.SAVE, config.getButtonList().get(0).getId());
        assertEquals(ButtonId.PIH_BASIC, config.getButtonList().get(1).getId());
        assertEquals(ButtonId.SHARE, config.getButtonList().get(2).getId());
    }
}
