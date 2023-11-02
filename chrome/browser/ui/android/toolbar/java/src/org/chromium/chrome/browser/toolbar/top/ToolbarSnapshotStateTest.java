// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.res.ColorStateList;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone.VisualState;
import org.chromium.chrome.browser.toolbar.top.ToolbarSnapshotState.ToolbarSnapshotDifference;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;

/** Unit tests for {@link ToolbarSnapshotState}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.DISABLE_COMPOSITED_PROGRESS_BAR)
public class ToolbarSnapshotStateTest {
    private static final @ColorInt int DEFAULT_TINT = Color.TRANSPARENT;
    private static final int DEFAULT_TAB_COUNT = 1;
    private static final ButtonData DEFAULT_BUTTON_DATA = makeButtonDate();
    private static final @VisualState int DEFAULT_VISUAL_STATE = VisualState.NORMAL;
    private static final String DEFAULT_URL_TEXT = "https://www.example.com/";
    private static final @DrawableRes int DEFAULT_SECURITY_ICON = 0;
    private static final boolean DEFAULT_IS_SHOWING_UPDATE_BADGE_DURING_LAST_CAPTURE = false;
    private static final boolean DEFAULT_IS_PAINT_PREVIEW = false;
    private static final float DEFAULT_PROGRESS = 0.1f;
    private static final int DEFAULT_UNFOCUSED_LOCATION_BAR_LAYOUT_WIDTH = 2;

    // Not static/final because they're initialized in #before(). Apparently ColorStateList.valueOf
    // calls into Android native code, and cannot be done too early.
    private ColorStateList mDefaultColorStateList;
    private ToolbarSnapshotState mDefaultToolbarSnapshotState;

    private static ButtonData makeButtonDate() {
        // Uses default equals impl, reference quality, to compare. Values do not matter.
        return new ButtonDataImpl(false, null, null, 0, false, null, false, 0);
    }

    @Before
    public void before() {
        mDefaultColorStateList = ColorStateList.valueOf(DEFAULT_TINT);
        mDefaultToolbarSnapshotState = new ToolbarSnapshotState(DEFAULT_TINT, DEFAULT_TAB_COUNT,
                DEFAULT_BUTTON_DATA, DEFAULT_VISUAL_STATE, DEFAULT_URL_TEXT, DEFAULT_SECURITY_ICON,
                mDefaultColorStateList, DEFAULT_IS_SHOWING_UPDATE_BADGE_DURING_LAST_CAPTURE,
                DEFAULT_IS_PAINT_PREVIEW, DEFAULT_PROGRESS,
                DEFAULT_UNFOCUSED_LOCATION_BAR_LAYOUT_WIDTH);
    }

    @Test
    public void testSameSnapshots() {
        ToolbarSnapshotState otherToolbarSnapshotState = new ToolbarSnapshotState(DEFAULT_TINT,
                DEFAULT_TAB_COUNT, DEFAULT_BUTTON_DATA, DEFAULT_VISUAL_STATE, DEFAULT_URL_TEXT,
                DEFAULT_SECURITY_ICON, mDefaultColorStateList,
                DEFAULT_IS_SHOWING_UPDATE_BADGE_DURING_LAST_CAPTURE, DEFAULT_IS_PAINT_PREVIEW,
                DEFAULT_PROGRESS, DEFAULT_UNFOCUSED_LOCATION_BAR_LAYOUT_WIDTH);
        Assert.assertEquals(ToolbarSnapshotDifference.NONE,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentTint() {
        ToolbarSnapshotState otherToolbarSnapshotState = new ToolbarSnapshotState(Color.RED,
                DEFAULT_TAB_COUNT, DEFAULT_BUTTON_DATA, DEFAULT_VISUAL_STATE, DEFAULT_URL_TEXT,
                DEFAULT_SECURITY_ICON, mDefaultColorStateList,
                DEFAULT_IS_SHOWING_UPDATE_BADGE_DURING_LAST_CAPTURE, DEFAULT_IS_PAINT_PREVIEW,
                DEFAULT_PROGRESS, DEFAULT_UNFOCUSED_LOCATION_BAR_LAYOUT_WIDTH);
        Assert.assertEquals(ToolbarSnapshotDifference.TINT,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentTabCount() {
        ToolbarSnapshotState otherToolbarSnapshotState = new ToolbarSnapshotState(DEFAULT_TINT, 2,
                DEFAULT_BUTTON_DATA, DEFAULT_VISUAL_STATE, DEFAULT_URL_TEXT, DEFAULT_SECURITY_ICON,
                mDefaultColorStateList, DEFAULT_IS_SHOWING_UPDATE_BADGE_DURING_LAST_CAPTURE,
                DEFAULT_IS_PAINT_PREVIEW, DEFAULT_PROGRESS,
                DEFAULT_UNFOCUSED_LOCATION_BAR_LAYOUT_WIDTH);
        Assert.assertEquals(ToolbarSnapshotDifference.TAB_COUNT,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentOptionalButtonData() {
        ToolbarSnapshotState otherToolbarSnapshotState = new ToolbarSnapshotState(DEFAULT_TINT,
                DEFAULT_TAB_COUNT, makeButtonDate(), DEFAULT_VISUAL_STATE, DEFAULT_URL_TEXT,
                DEFAULT_SECURITY_ICON, mDefaultColorStateList,
                DEFAULT_IS_SHOWING_UPDATE_BADGE_DURING_LAST_CAPTURE, DEFAULT_IS_PAINT_PREVIEW,
                DEFAULT_PROGRESS, DEFAULT_UNFOCUSED_LOCATION_BAR_LAYOUT_WIDTH);
        Assert.assertEquals(ToolbarSnapshotDifference.OPTIONAL_BUTTON_DATA,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentVisualState() {
        ToolbarSnapshotState otherToolbarSnapshotState = new ToolbarSnapshotState(DEFAULT_TINT,
                DEFAULT_TAB_COUNT, DEFAULT_BUTTON_DATA, VisualState.INCOGNITO, DEFAULT_URL_TEXT,
                DEFAULT_SECURITY_ICON, mDefaultColorStateList,
                DEFAULT_IS_SHOWING_UPDATE_BADGE_DURING_LAST_CAPTURE, DEFAULT_IS_PAINT_PREVIEW,
                DEFAULT_PROGRESS, DEFAULT_UNFOCUSED_LOCATION_BAR_LAYOUT_WIDTH);
        Assert.assertEquals(ToolbarSnapshotDifference.VISUAL_STATE,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentUrlText() {
        ToolbarSnapshotState otherToolbarSnapshotState = new ToolbarSnapshotState(DEFAULT_TINT,
                DEFAULT_TAB_COUNT, DEFAULT_BUTTON_DATA, DEFAULT_VISUAL_STATE,
                "https://www.other.com/", DEFAULT_SECURITY_ICON, mDefaultColorStateList,
                DEFAULT_IS_SHOWING_UPDATE_BADGE_DURING_LAST_CAPTURE, DEFAULT_IS_PAINT_PREVIEW,
                DEFAULT_PROGRESS, DEFAULT_UNFOCUSED_LOCATION_BAR_LAYOUT_WIDTH);
        Assert.assertEquals(ToolbarSnapshotDifference.URL_TEXT,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentSecurityIcon() {
        ToolbarSnapshotState otherToolbarSnapshotState = new ToolbarSnapshotState(DEFAULT_TINT,
                DEFAULT_TAB_COUNT, DEFAULT_BUTTON_DATA, DEFAULT_VISUAL_STATE, DEFAULT_URL_TEXT, -1,
                mDefaultColorStateList, DEFAULT_IS_SHOWING_UPDATE_BADGE_DURING_LAST_CAPTURE,
                DEFAULT_IS_PAINT_PREVIEW, DEFAULT_PROGRESS,
                DEFAULT_UNFOCUSED_LOCATION_BAR_LAYOUT_WIDTH);
        Assert.assertEquals(ToolbarSnapshotDifference.SECURITY_ICON,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentColorStateList() {
        ToolbarSnapshotState otherToolbarSnapshotState = new ToolbarSnapshotState(DEFAULT_TINT,
                DEFAULT_TAB_COUNT, DEFAULT_BUTTON_DATA, DEFAULT_VISUAL_STATE, DEFAULT_URL_TEXT,
                DEFAULT_SECURITY_ICON, ColorStateList.valueOf(Color.RED),
                DEFAULT_IS_SHOWING_UPDATE_BADGE_DURING_LAST_CAPTURE, DEFAULT_IS_PAINT_PREVIEW,
                DEFAULT_PROGRESS, DEFAULT_UNFOCUSED_LOCATION_BAR_LAYOUT_WIDTH);
        Assert.assertEquals(ToolbarSnapshotDifference.HOME_BUTTON_COLOR,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentIsShowingUpdateBadgeDuringLastCapture() {
        ToolbarSnapshotState otherToolbarSnapshotState = new ToolbarSnapshotState(DEFAULT_TINT,
                DEFAULT_TAB_COUNT, DEFAULT_BUTTON_DATA, DEFAULT_VISUAL_STATE, DEFAULT_URL_TEXT,
                DEFAULT_SECURITY_ICON, mDefaultColorStateList, true, DEFAULT_IS_PAINT_PREVIEW,
                DEFAULT_PROGRESS, DEFAULT_UNFOCUSED_LOCATION_BAR_LAYOUT_WIDTH);
        Assert.assertEquals(ToolbarSnapshotDifference.SHOWING_UPDATE_BADGE,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentIsPaintPreview() {
        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotState(DEFAULT_TINT, DEFAULT_TAB_COUNT, DEFAULT_BUTTON_DATA,
                        DEFAULT_VISUAL_STATE, DEFAULT_URL_TEXT, DEFAULT_SECURITY_ICON,
                        mDefaultColorStateList, DEFAULT_IS_SHOWING_UPDATE_BADGE_DURING_LAST_CAPTURE,
                        true, DEFAULT_PROGRESS, DEFAULT_UNFOCUSED_LOCATION_BAR_LAYOUT_WIDTH);
        Assert.assertEquals(ToolbarSnapshotDifference.PAINT_PREVIEW,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }
    @Test
    public void testDifferentProgress() {
        ToolbarSnapshotState otherToolbarSnapshotState = new ToolbarSnapshotState(DEFAULT_TINT,
                DEFAULT_TAB_COUNT, DEFAULT_BUTTON_DATA, DEFAULT_VISUAL_STATE, DEFAULT_URL_TEXT,
                DEFAULT_SECURITY_ICON, mDefaultColorStateList,
                DEFAULT_IS_SHOWING_UPDATE_BADGE_DURING_LAST_CAPTURE, DEFAULT_IS_PAINT_PREVIEW, 0.2f,
                DEFAULT_UNFOCUSED_LOCATION_BAR_LAYOUT_WIDTH);
        Assert.assertEquals(ToolbarSnapshotDifference.NONE,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }
    @Test
    public void testDifferentUnfocusedLocationBarLayoutWidth() {
        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotState(DEFAULT_TINT, DEFAULT_TAB_COUNT, DEFAULT_BUTTON_DATA,
                        DEFAULT_VISUAL_STATE, DEFAULT_URL_TEXT, DEFAULT_SECURITY_ICON,
                        mDefaultColorStateList, DEFAULT_IS_SHOWING_UPDATE_BADGE_DURING_LAST_CAPTURE,
                        DEFAULT_IS_PAINT_PREVIEW, DEFAULT_PROGRESS, 100);
        Assert.assertEquals(ToolbarSnapshotDifference.LOCATION_BAR_WIDTH,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }
}