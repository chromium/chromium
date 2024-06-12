// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone.VisualState;

/** Unit tests for {@link PhoneCaptureStateToken}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PhoneCaptureStateTokenTest {
    private static final @ColorInt int DEFAULT_TINT = Color.TRANSPARENT;
    private static final int DEFAULT_TAB_COUNT = 1;
    private static final ButtonData DEFAULT_BUTTON_DATA = makeButtonDate();
    private static final @VisualState int DEFAULT_VISUAL_STATE = VisualState.NORMAL;
    private static final String DEFAULT_URL_TEXT = "https://www.example.com/";
    private static final CharSequence DEFAULT_URL_HINT_TEXT = null;
    private static final @DrawableRes int DEFAULT_SECURITY_ICON = 0;
    private static final boolean DEFAULT_HOME_BUTTON_IS_VISIBLE = true;
    private static final boolean DEFAULT_IS_SHOWING_UPDATE_BADGE_DURING_LAST_CAPTURE = false;
    private static final boolean DEFAULT_IS_PAINT_PREVIEW = false;
    private static final float DEFAULT_PROGRESS = 0.1f;
    private static final int DEFAULT_UNFOCUSED_LOCATION_BAR_LAYOUT_WIDTH = 2;

    // Not static/final because they're initialized in #before(). Apparently ColorStateList.valueOf
    // calls into Android native code, and cannot be done too early.
    private ColorStateList mDefaultHomeButtonColorStateList;
    private PhoneCaptureStateToken mDefaultPhoneCaptureStateToken;

    private static ButtonData makeButtonDate() {
        // Uses default equals impl, reference quality, to compare. Values do not matter.
        return new ButtonDataImpl(false, null, null, "", false, null, false, 0, 0, false);
    }

    @Before
    public void before() {
        mDefaultHomeButtonColorStateList = ColorStateList.valueOf(DEFAULT_TINT);
        mDefaultPhoneCaptureStateToken = new PhoneCustomTabCaptureStateTokenBuilder().build();
    }

    @Test
    public void testSameSnapshots() {
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder().build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.NONE,
                PhoneCaptureStateToken.getAnyDifference(
                        mDefaultPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testDifferentTint() {
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder().setTint(Color.RED).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.TINT,
                PhoneCaptureStateToken.getAnyDifference(
                        mDefaultPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testDifferentTabCount() {
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder().setTabCount(2).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.TAB_COUNT,
                PhoneCaptureStateToken.getAnyDifference(
                        mDefaultPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testDifferentOptionalButtonData() {
        ButtonDataImpl otherButtonData = (ButtonDataImpl) makeButtonDate();
        otherButtonData.setCanShow(!otherButtonData.canShow());
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder()
                        .setOptionalButtonData(otherButtonData)
                        .build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.OPTIONAL_BUTTON,
                PhoneCaptureStateToken.getAnyDifference(
                        mDefaultPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testNullOptionalButtonData() {
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder().setOptionalButtonData(null).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.OPTIONAL_BUTTON,
                PhoneCaptureStateToken.getAnyDifference(
                        mDefaultPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testSameOptionalButtonData_DifferentDrawable() {
        ButtonDataImpl buttonData = (ButtonDataImpl) makeButtonDate();
        // Create a capture state with the original ButtonData.
        PhoneCaptureStateToken initialPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder()
                        .setOptionalButtonData(buttonData)
                        .build();
        buttonData.updateDrawable(new ColorDrawable());
        // Then create another capture when the button's drawable gets updated.
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder()
                        .setOptionalButtonData(buttonData)
                        .build();

        Assert.assertEquals(
                ToolbarSnapshotDifference.OPTIONAL_BUTTON,
                PhoneCaptureStateToken.getAnyDifference(
                        initialPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testDifferentVisualState() {
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder()
                        .setVisualState(VisualState.INCOGNITO)
                        .build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.VISUAL_STATE,
                PhoneCaptureStateToken.getAnyDifference(
                        mDefaultPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testDifferentUrlText() {
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder()
                        .setUrlText("https://www.other.com/")
                        .build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.URL_TEXT,
                PhoneCaptureStateToken.getAnyDifference(
                        mDefaultPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testDifferentUrlText_SameHintText() {
        PhoneCaptureStateToken initialPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder()
                        .setVisibleTextPrefixHint(DEFAULT_URL_TEXT)
                        .build();
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder()
                        .setUrlText(DEFAULT_URL_TEXT + "additional/paths/")
                        .setVisibleTextPrefixHint(DEFAULT_URL_TEXT)
                        .build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.NONE,
                PhoneCaptureStateToken.getAnyDifference(
                        initialPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testSameUrlText_DifferentHintText() {
        PhoneCaptureStateToken initialPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder()
                        .setVisibleTextPrefixHint(DEFAULT_URL_TEXT.substring(0, 2))
                        .build();
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder()
                        .setVisibleTextPrefixHint(DEFAULT_URL_TEXT.substring(0, 3))
                        .build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.NONE,
                PhoneCaptureStateToken.getAnyDifference(
                        initialPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testSameUrlText_BothNullHintText() {
        PhoneCaptureStateToken initialPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder().setVisibleTextPrefixHint(null).build();
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder().setVisibleTextPrefixHint(null).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.NONE,
                PhoneCaptureStateToken.getAnyDifference(
                        initialPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testSameUrlText_NullHintText() {
        PhoneCaptureStateToken initialPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder().setVisibleTextPrefixHint(null).build();
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder()
                        .setVisibleTextPrefixHint(DEFAULT_URL_TEXT)
                        .build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.NONE,
                PhoneCaptureStateToken.getAnyDifference(
                        initialPhoneCaptureStateToken, otherPhoneCaptureStateToken));
        Assert.assertEquals(
                ToolbarSnapshotDifference.NONE,
                PhoneCaptureStateToken.getAnyDifference(
                        otherPhoneCaptureStateToken, initialPhoneCaptureStateToken));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_NO_VISIBLE_HINT_FOR_DIFFERENT_TLD)
    public void testCompareHintToPreviousUrl() {
        String latestVisibleHint = DEFAULT_URL_TEXT + "aaaaa";
        PhoneCaptureStateToken initialPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder()
                        .setUrlText(latestVisibleHint + "foo")
                        .setVisibleTextPrefixHint(null)
                        .build();
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder()
                        .setUrlText(latestVisibleHint + "bar")
                        .setVisibleTextPrefixHint(latestVisibleHint)
                        .build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.NONE,
                PhoneCaptureStateToken.getAnyDifference(
                        initialPhoneCaptureStateToken, otherPhoneCaptureStateToken));
        Assert.assertEquals(
                ToolbarSnapshotDifference.URL_TEXT,
                PhoneCaptureStateToken.getAnyDifference(
                        otherPhoneCaptureStateToken, initialPhoneCaptureStateToken));
    }

    @Test
    public void testDifferentSecurityIcon() {
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder().setSecurityIcon(-1).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.SECURITY_ICON,
                PhoneCaptureStateToken.getAnyDifference(
                        mDefaultPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testDifferentHomeButtonColorStateList() {
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder()
                        .setHomeButtonColorStateList(ColorStateList.valueOf(Color.RED))
                        .build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.HOME_BUTTON,
                PhoneCaptureStateToken.getAnyDifference(
                        mDefaultPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testDifferentHomeButtonIsVisible() {
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder().setHomeButtonIsVisible(false).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.HOME_BUTTON,
                PhoneCaptureStateToken.getAnyDifference(
                        mDefaultPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testSameColorStateList() {
        // Create the ColorStateList by hand. ColorStateList.valueOf will inconsistently reuse
        // objects, but this ColorStateList should never have reference equality with the default.
        ColorStateList colorStateList =
                new ColorStateList(new int[][] {new int[] {}}, new int[] {DEFAULT_TINT});
        Assert.assertNotEquals(mDefaultHomeButtonColorStateList, colorStateList);

        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder()
                        .setHomeButtonColorStateList(colorStateList)
                        .build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.NONE,
                PhoneCaptureStateToken.getAnyDifference(
                        mDefaultPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testDifferentIsShowingUpdateBadgeDuringLastCapture() {
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder()
                        .setIsShowingUpdateBadgeDuringLastCapture(true)
                        .build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.SHOWING_UPDATE_BADGE,
                PhoneCaptureStateToken.getAnyDifference(
                        mDefaultPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testDifferentIsPaintPreview() {
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder().setIsPaintPreview(true).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.PAINT_PREVIEW,
                PhoneCaptureStateToken.getAnyDifference(
                        mDefaultPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testDifferentProgress() {
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder().setProgress(0.2f).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.NONE,
                PhoneCaptureStateToken.getAnyDifference(
                        mDefaultPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    @Test
    public void testDifferentUnfocusedLocationBarLayoutWidth() {
        PhoneCaptureStateToken otherPhoneCaptureStateToken =
                new PhoneCustomTabCaptureStateTokenBuilder()
                        .setUnfocusedLocationBarLayoutWidth(100)
                        .build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.LOCATION_BAR_WIDTH,
                PhoneCaptureStateToken.getAnyDifference(
                        mDefaultPhoneCaptureStateToken, otherPhoneCaptureStateToken));
    }

    private class PhoneCustomTabCaptureStateTokenBuilder {
        private @ColorInt int mTint = DEFAULT_TINT;
        private int mTabCount = DEFAULT_TAB_COUNT;
        private ButtonData mOptionalButtonData = DEFAULT_BUTTON_DATA;
        private @VisualState int mVisualState = DEFAULT_VISUAL_STATE;
        private String mUrlText = DEFAULT_URL_TEXT;
        private @Nullable CharSequence mVisibleTextPrefixHint = DEFAULT_URL_HINT_TEXT;
        private @DrawableRes int mSecurityIcon = DEFAULT_SECURITY_ICON;
        private ColorStateList mHomeButtonColorStateList = mDefaultHomeButtonColorStateList;
        private boolean mHomeButtonIsVisible = DEFAULT_HOME_BUTTON_IS_VISIBLE;
        private boolean mIsShowingUpdateBadgeDuringLastCapture =
                DEFAULT_IS_SHOWING_UPDATE_BADGE_DURING_LAST_CAPTURE;
        private boolean mIsPaintPreview = DEFAULT_IS_PAINT_PREVIEW;
        private float mProgress = DEFAULT_PROGRESS;
        private int mUnfocusedLocationBarLayoutWidth = DEFAULT_UNFOCUSED_LOCATION_BAR_LAYOUT_WIDTH;

        public PhoneCustomTabCaptureStateTokenBuilder setTint(@ColorInt int tint) {
            mTint = tint;
            return this;
        }

        public PhoneCustomTabCaptureStateTokenBuilder setTabCount(int tabCount) {
            mTabCount = tabCount;
            return this;
        }

        public PhoneCustomTabCaptureStateTokenBuilder setOptionalButtonData(
                ButtonData optionalButtonData) {
            mOptionalButtonData = optionalButtonData;
            return this;
        }

        public PhoneCustomTabCaptureStateTokenBuilder setVisualState(@VisualState int visualState) {
            mVisualState = visualState;
            return this;
        }

        public PhoneCustomTabCaptureStateTokenBuilder setUrlText(String urlText) {
            mUrlText = urlText;
            return this;
        }

        public PhoneCustomTabCaptureStateTokenBuilder setVisibleTextPrefixHint(
                CharSequence visibleTextPrefixHint) {
            mVisibleTextPrefixHint = visibleTextPrefixHint;
            return this;
        }

        public PhoneCustomTabCaptureStateTokenBuilder setSecurityIcon(
                @DrawableRes int securityIcon) {
            mSecurityIcon = securityIcon;
            return this;
        }

        public PhoneCustomTabCaptureStateTokenBuilder setHomeButtonColorStateList(
                ColorStateList homeButtonColorStateList) {
            mHomeButtonColorStateList = homeButtonColorStateList;
            return this;
        }

        public PhoneCustomTabCaptureStateTokenBuilder setHomeButtonIsVisible(
                boolean homeButtonIsVisible) {
            mHomeButtonIsVisible = homeButtonIsVisible;
            return this;
        }

        public PhoneCustomTabCaptureStateTokenBuilder setIsShowingUpdateBadgeDuringLastCapture(
                boolean isShowingUpdateBadgeDuringLastCapture) {
            mIsShowingUpdateBadgeDuringLastCapture = isShowingUpdateBadgeDuringLastCapture;
            return this;
        }

        public PhoneCustomTabCaptureStateTokenBuilder setIsPaintPreview(boolean isPaintPreview) {
            mIsPaintPreview = isPaintPreview;
            return this;
        }

        public PhoneCustomTabCaptureStateTokenBuilder setProgress(float progress) {
            mProgress = progress;
            return this;
        }

        public PhoneCustomTabCaptureStateTokenBuilder setUnfocusedLocationBarLayoutWidth(
                int unfocusedLocationBarLayoutWidth) {
            mUnfocusedLocationBarLayoutWidth = unfocusedLocationBarLayoutWidth;
            return this;
        }

        public PhoneCaptureStateToken build() {
            VisibleUrlText visibleUrlText = new VisibleUrlText(mUrlText, mVisibleTextPrefixHint);
            return new PhoneCaptureStateToken(
                    mTint,
                    mTabCount,
                    mOptionalButtonData,
                    mVisualState,
                    visibleUrlText,
                    mSecurityIcon,
                    mHomeButtonColorStateList,
                    mHomeButtonIsVisible,
                    mIsShowingUpdateBadgeDuringLastCapture,
                    mIsPaintPreview,
                    mProgress,
                    mUnfocusedLocationBarLayoutWidth);
        }
    }
}
