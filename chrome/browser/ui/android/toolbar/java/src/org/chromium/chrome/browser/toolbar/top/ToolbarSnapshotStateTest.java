// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.res.ColorStateList;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone.VisualState;
import org.chromium.chrome.browser.toolbar.top.ToolbarSnapshotState.ToolbarSnapshotDifference;

/** Unit tests for {@link ToolbarSnapshotState}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ToolbarSnapshotStateTest {
    private static final @ColorInt int DEFAULT_TINT = Color.TRANSPARENT;
    private static final int DEFAULT_TAB_COUNT = 1;
    private static final ButtonData DEFAULT_BUTTON_DATA = makeButtonDate();
    private static final @VisualState int DEFAULT_VISUAL_STATE = VisualState.NORMAL;
    private static final String DEFAULT_URL_TEXT = "https://www.example.com/";
    private static final CharSequence DEFAULT_URL_HINT_TEXT = null;
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
        mDefaultToolbarSnapshotState = new ToolbarSnapshotStateBuilder().build();
    }

    @Test
    public void testSameSnapshots() {
        ToolbarSnapshotState otherToolbarSnapshotState = new ToolbarSnapshotStateBuilder().build();
        Assert.assertEquals(ToolbarSnapshotDifference.NONE,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentTint() {
        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder().setTint(Color.RED).build();
        Assert.assertEquals(ToolbarSnapshotDifference.TINT,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentTabCount() {
        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder().setTabCount(2).build();
        Assert.assertEquals(ToolbarSnapshotDifference.TAB_COUNT,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentOptionalButtonData() {
        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder().setOptionalButtonData(makeButtonDate()).build();
        Assert.assertEquals(ToolbarSnapshotDifference.OPTIONAL_BUTTON_DATA,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentVisualState() {
        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder().setVisualState(VisualState.INCOGNITO).build();
        Assert.assertEquals(ToolbarSnapshotDifference.VISUAL_STATE,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentUrlText() {
        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder().setUrlText("https://www.other.com/").build();
        Assert.assertEquals(ToolbarSnapshotDifference.URL_TEXT,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentUrlText_SameHintText() {
        ToolbarSnapshotState initialToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder()
                        .setVisibleTextPrefixHint(DEFAULT_URL_TEXT)
                        .build();
        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder()
                        .setUrlText(DEFAULT_URL_TEXT + "additional/paths/")
                        .setVisibleTextPrefixHint(DEFAULT_URL_TEXT)
                        .build();
        Assert.assertEquals(ToolbarSnapshotDifference.NONE,
                initialToolbarSnapshotState.getAnyDifference(otherToolbarSnapshotState));
    }

    @Test
    public void testSameUrlText_DifferentHintText() {
        ToolbarSnapshotState initialToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder()
                        .setVisibleTextPrefixHint(DEFAULT_URL_TEXT.substring(0, 2))
                        .build();
        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder()
                        .setVisibleTextPrefixHint(DEFAULT_URL_TEXT.substring(0, 3))
                        .build();
        Assert.assertEquals(ToolbarSnapshotDifference.NONE,
                initialToolbarSnapshotState.getAnyDifference(otherToolbarSnapshotState));
    }

    @Test
    public void testSameUrlText_BothNullHintText() {
        ToolbarSnapshotState initialToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder().setVisibleTextPrefixHint(null).build();
        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder().setVisibleTextPrefixHint(null).build();
        Assert.assertEquals(ToolbarSnapshotDifference.NONE,
                initialToolbarSnapshotState.getAnyDifference(otherToolbarSnapshotState));
    }

    @Test
    public void testSameUrlText_NullHintText() {
        ToolbarSnapshotState initialToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder().setVisibleTextPrefixHint(null).build();
        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder()
                        .setVisibleTextPrefixHint(DEFAULT_URL_TEXT)
                        .build();
        Assert.assertEquals(ToolbarSnapshotDifference.NONE,
                initialToolbarSnapshotState.getAnyDifference(otherToolbarSnapshotState));
        Assert.assertEquals(ToolbarSnapshotDifference.NONE,
                otherToolbarSnapshotState.getAnyDifference(initialToolbarSnapshotState));
    }

    @Test
    public void testDifferentSecurityIcon() {
        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder().setSecurityIcon(-1).build();
        Assert.assertEquals(ToolbarSnapshotDifference.SECURITY_ICON,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentColorStateList() {
        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder()
                        .setColorStateList(ColorStateList.valueOf(Color.RED))
                        .build();
        Assert.assertEquals(ToolbarSnapshotDifference.HOME_BUTTON_COLOR,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testSameColorStateList() {
        // Create the ColorStateList by hand. ColorStateList.valueOf will inconsistently reuse
        // objects, but this ColorStateList should never have reference equality with the default.
        ColorStateList colorStateList =
                new ColorStateList(new int[][] {new int[] {}}, new int[] {DEFAULT_TINT});
        Assert.assertNotEquals(mDefaultColorStateList, colorStateList);

        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder().setColorStateList(colorStateList).build();
        Assert.assertEquals(ToolbarSnapshotDifference.NONE,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentIsShowingUpdateBadgeDuringLastCapture() {
        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder()
                        .setIsShowingUpdateBadgeDuringLastCapture(true)
                        .build();
        Assert.assertEquals(ToolbarSnapshotDifference.SHOWING_UPDATE_BADGE,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentIsPaintPreview() {
        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder().setIsPaintPreview(true).build();
        Assert.assertEquals(ToolbarSnapshotDifference.PAINT_PREVIEW,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentProgress() {
        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder().setProgress(0.2f).build();
        Assert.assertEquals(ToolbarSnapshotDifference.NONE,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testDifferentUnfocusedLocationBarLayoutWidth() {
        ToolbarSnapshotState otherToolbarSnapshotState =
                new ToolbarSnapshotStateBuilder().setUnfocusedLocationBarLayoutWidth(100).build();
        Assert.assertEquals(ToolbarSnapshotDifference.LOCATION_BAR_WIDTH,
                otherToolbarSnapshotState.getAnyDifference(mDefaultToolbarSnapshotState));
    }

    @Test
    public void testIsValidVisibleTextPrefixHint() {
        Assert.assertFalse(ToolbarSnapshotState.isValidVisibleTextPrefixHint(null, null));
        Assert.assertFalse(ToolbarSnapshotState.isValidVisibleTextPrefixHint("foo", null));
        Assert.assertFalse(ToolbarSnapshotState.isValidVisibleTextPrefixHint(null, "foo"));

        Assert.assertFalse(ToolbarSnapshotState.isValidVisibleTextPrefixHint("", ""));
        Assert.assertFalse(ToolbarSnapshotState.isValidVisibleTextPrefixHint("foo", ""));

        Assert.assertFalse(ToolbarSnapshotState.isValidVisibleTextPrefixHint("foo", "fooo"));
        Assert.assertFalse(ToolbarSnapshotState.isValidVisibleTextPrefixHint("foo", "foo/"));
        Assert.assertFalse(ToolbarSnapshotState.isValidVisibleTextPrefixHint("foo", "o/"));
        Assert.assertFalse(ToolbarSnapshotState.isValidVisibleTextPrefixHint("foo", "oo"));

        Assert.assertTrue(ToolbarSnapshotState.isValidVisibleTextPrefixHint("foo.com", "foo"));
        Assert.assertTrue(ToolbarSnapshotState.isValidVisibleTextPrefixHint("foo.com", "foo.com"));
    }

    private class ToolbarSnapshotStateBuilder {
        private @ColorInt int mTint = DEFAULT_TINT;
        private int mTabCount = DEFAULT_TAB_COUNT;
        private ButtonData mOptionalButtonData = DEFAULT_BUTTON_DATA;
        private @VisualState int mVisualState = DEFAULT_VISUAL_STATE;
        private String mUrlText = DEFAULT_URL_TEXT;
        @Nullable
        private CharSequence mVisibleTextPrefixHint = DEFAULT_URL_HINT_TEXT;
        private @DrawableRes int mSecurityIcon = DEFAULT_SECURITY_ICON;
        private ColorStateList mColorStateList = mDefaultColorStateList;
        private boolean mIsShowingUpdateBadgeDuringLastCapture =
                DEFAULT_IS_SHOWING_UPDATE_BADGE_DURING_LAST_CAPTURE;
        private boolean mIsPaintPreview = DEFAULT_IS_PAINT_PREVIEW;
        private float mProgress = DEFAULT_PROGRESS;
        private int mUnfocusedLocationBarLayoutWidth = DEFAULT_UNFOCUSED_LOCATION_BAR_LAYOUT_WIDTH;

        public ToolbarSnapshotStateBuilder setTint(@ColorInt int tint) {
            mTint = tint;
            return this;
        }

        public ToolbarSnapshotStateBuilder setTabCount(int tabCount) {
            mTabCount = tabCount;
            return this;
        }

        public ToolbarSnapshotStateBuilder setOptionalButtonData(ButtonData optionalButtonData) {
            mOptionalButtonData = optionalButtonData;
            return this;
        }

        public ToolbarSnapshotStateBuilder setVisualState(@VisualState int visualState) {
            mVisualState = visualState;
            return this;
        }

        public ToolbarSnapshotStateBuilder setUrlText(String urlText) {
            mUrlText = urlText;
            return this;
        }

        public ToolbarSnapshotStateBuilder setVisibleTextPrefixHint(
                CharSequence visibleTextPrefixHint) {
            mVisibleTextPrefixHint = visibleTextPrefixHint;
            return this;
        }

        public ToolbarSnapshotStateBuilder setSecurityIcon(@DrawableRes int securityIcon) {
            mSecurityIcon = securityIcon;
            return this;
        }

        public ToolbarSnapshotStateBuilder setColorStateList(ColorStateList colorStateList) {
            mColorStateList = colorStateList;
            return this;
        }

        public ToolbarSnapshotStateBuilder setIsShowingUpdateBadgeDuringLastCapture(
                boolean isShowingUpdateBadgeDuringLastCapture) {
            mIsShowingUpdateBadgeDuringLastCapture = isShowingUpdateBadgeDuringLastCapture;
            return this;
        }

        public ToolbarSnapshotStateBuilder setIsPaintPreview(boolean isPaintPreview) {
            mIsPaintPreview = isPaintPreview;
            return this;
        }

        public ToolbarSnapshotStateBuilder setProgress(float progress) {
            mProgress = progress;
            return this;
        }

        public ToolbarSnapshotStateBuilder setUnfocusedLocationBarLayoutWidth(
                int unfocusedLocationBarLayoutWidth) {
            mUnfocusedLocationBarLayoutWidth = unfocusedLocationBarLayoutWidth;
            return this;
        }

        public ToolbarSnapshotState build() {
            return new ToolbarSnapshotState(mTint, mTabCount, mOptionalButtonData, mVisualState,
                    mUrlText, mVisibleTextPrefixHint, mSecurityIcon, mColorStateList,
                    mIsShowingUpdateBadgeDuringLastCapture, mIsPaintPreview, mProgress,
                    mUnfocusedLocationBarLayoutWidth);
        }
    }
}
