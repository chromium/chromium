// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.mockito.Mockito.when;

import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageButton;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TabletCaptureStateToken}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabletCaptureStateTokenTest {
    private static final Drawable DEFAULT_HOME_BUTTON_DRAWABLE = Mockito.mock(Drawable.class);
    private static final Drawable DEFAULT_BACKWARD_BUTTON_DRAWABLE = Mockito.mock(Drawable.class);
    private static final Drawable DEFAULT_FORWARD_BUTTON_DRAWABLE = Mockito.mock(Drawable.class);
    private static final Drawable DEFAULT_RELOAD_BUTTON_DRAWABLE = Mockito.mock(Drawable.class);
    private static final Drawable DEFAULT_OPTIONAL_BUTTON_DRAWABLE = Mockito.mock(Drawable.class);

    private static final boolean DEFAULT_HAS_IMAGE_TINT_LIST = true;
    private static final int DEFAULT_VISIBILITY = View.VISIBLE;
    private static final boolean DEFAULT_IS_ENABLED = true;
    private static final int DEFAULT_LEVEL = 0;
    private static final @ColorInt int DEFAULT_COLOR = Color.RED;
    private static final @DrawableRes int DEFAULT_ICON_RES = 0;

    private static final @DrawableRes int DEFAULT_SECURITY_ICON = 0;
    private static final VisibleUrlText DEFAULT_VISIBLE_URL_TEXT =
            new VisibleUrlText("https://www.example.com/", null);
    private static final @DrawableRes int DEFAULT_BOOKMARK_ICON = 0;
    private static final int DEFAULT_TAB_COUNT = 1;
    private static final int DEFAULT_VIEW_WIDTH = 100;

    private final TabletCaptureStateToken mDefaultTabletToken =
            new TabletCaptureStateTokenBuilder().build();

    private static class MockImageButtonBuilder {
        private final ImageButton mImageButton;
        private final ColorStateList mColorStateList;

        private Drawable mDrawable;
        private boolean mHasImageTintList = DEFAULT_HAS_IMAGE_TINT_LIST;
        private int mVisibility = DEFAULT_VISIBILITY;
        private boolean mIsEnabled = DEFAULT_IS_ENABLED;
        private int mLevel = DEFAULT_LEVEL;
        private int mColor = DEFAULT_COLOR;

        MockImageButtonBuilder() {
            mImageButton = Mockito.mock(ImageButton.class);
            mColorStateList = Mockito.mock(ColorStateList.class);
        }

        MockImageButtonBuilder withDrawable(Drawable drawable) {
            mDrawable = drawable;
            return this;
        }

        MockImageButtonBuilder withHasImageTintList(boolean hasDrawable) {
            mHasImageTintList = hasDrawable;
            return this;
        }

        MockImageButtonBuilder withVisibility(int visibility) {
            mVisibility = visibility;
            return this;
        }

        MockImageButtonBuilder withIsEnabled(boolean isEnabled) {
            mIsEnabled = isEnabled;
            return this;
        }

        MockImageButtonBuilder withLevel(int level) {
            mLevel = level;
            return this;
        }

        MockImageButtonBuilder withColor(int color) {
            mColor = color;
            return this;
        }

        ImageButton build() {
            when(mImageButton.getDrawable()).thenReturn(mDrawable);
            when(mImageButton.getImageTintList())
                    .thenReturn(mHasImageTintList ? mColorStateList : null);
            when(mImageButton.getVisibility()).thenReturn(mVisibility);
            when(mImageButton.isEnabled()).thenReturn(mIsEnabled);
            if (mDrawable != null) {
                when(mDrawable.getLevel()).thenReturn(mLevel);
            }
            when(mColorStateList.getDefaultColor()).thenReturn(mColor);
            return mImageButton;
        }
    }

    private static class TabletCaptureStateTokenBuilder {
        private ImageButton mHomeButton =
                new MockImageButtonBuilder().withDrawable(DEFAULT_HOME_BUTTON_DRAWABLE).build();
        private ImageButton mBackwardButton =
                new MockImageButtonBuilder().withDrawable(DEFAULT_BACKWARD_BUTTON_DRAWABLE).build();
        private ImageButton mForwardButton =
                new MockImageButtonBuilder().withDrawable(DEFAULT_FORWARD_BUTTON_DRAWABLE).build();
        private ImageButton mReloadButton =
                new MockImageButtonBuilder().withDrawable(DEFAULT_RELOAD_BUTTON_DRAWABLE).build();
        private @DrawableRes int mSecurityIcon = DEFAULT_SECURITY_ICON;
        private VisibleUrlText mVisibleUrlText = DEFAULT_VISIBLE_URL_TEXT;
        private ImageButton mBookmarkButton = new MockImageButtonBuilder().build();
        private @DrawableRes int mBookmarkIconRes = DEFAULT_BOOKMARK_ICON;
        private ImageButton mOptionalButton =
                new MockImageButtonBuilder().withDrawable(DEFAULT_OPTIONAL_BUTTON_DRAWABLE).build();
        private int mTabCount = DEFAULT_TAB_COUNT;
        private int mViewWidth = DEFAULT_VIEW_WIDTH;

        TabletCaptureStateTokenBuilder withHomeButton(ImageButton button) {
            mHomeButton = button;
            return this;
        }

        TabletCaptureStateTokenBuilder withBackwardButton(ImageButton button) {
            mBackwardButton = button;
            return this;
        }

        TabletCaptureStateTokenBuilder withForwardButton(ImageButton button) {
            mForwardButton = button;
            return this;
        }

        TabletCaptureStateTokenBuilder withReloadButton(ImageButton button) {
            mReloadButton = button;
            return this;
        }

        TabletCaptureStateTokenBuilder withSecurityIcon(@DrawableRes int securityIcon) {
            mSecurityIcon = securityIcon;
            return this;
        }

        TabletCaptureStateTokenBuilder withVisibleUrlText(VisibleUrlText visibleUrlText) {
            mVisibleUrlText = visibleUrlText;
            return this;
        }

        TabletCaptureStateTokenBuilder withBookmarkButton(ImageButton button) {
            mBookmarkButton = button;
            return this;
        }

        TabletCaptureStateTokenBuilder withBookmarkIconRes(@DrawableRes int iconRes) {
            mBookmarkIconRes = iconRes;
            return this;
        }

        TabletCaptureStateTokenBuilder withOptionalButton(ImageButton button) {
            mOptionalButton = button;
            return this;
        }

        TabletCaptureStateTokenBuilder withTabCount(int tabCount) {
            mTabCount = tabCount;
            return this;
        }

        TabletCaptureStateTokenBuilder withViewWidth(int viewWidth) {
            mViewWidth = viewWidth;
            return this;
        }

        TabletCaptureStateToken build() {
            return new TabletCaptureStateToken(
                    mHomeButton,
                    mBackwardButton,
                    mForwardButton,
                    mReloadButton,
                    mSecurityIcon,
                    mVisibleUrlText,
                    mBookmarkButton,
                    mBookmarkIconRes,
                    mOptionalButton,
                    mTabCount,
                    mViewWidth);
        }
    }

    @Test
    public void testSameSnapshots() {
        TabletCaptureStateToken tabletToken = new TabletCaptureStateTokenBuilder().build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.NONE, tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentNull() {
        Assert.assertEquals(
                ToolbarSnapshotDifference.NULL, mDefaultTabletToken.getAnyDifference(null));
    }

    @Test
    public void testDifferentHomeButton() {
        ImageButton button =
                new MockImageButtonBuilder()
                        .withDrawable(DEFAULT_HOME_BUTTON_DRAWABLE)
                        .withVisibility(View.INVISIBLE)
                        .build();
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withHomeButton(button).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.HOME_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentBackButton() {
        ImageButton button =
                new MockImageButtonBuilder()
                        .withDrawable(DEFAULT_BACKWARD_BUTTON_DRAWABLE)
                        .withIsEnabled(false)
                        .build();
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withBackwardButton(button).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.BACK_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentForwardButton() {
        ImageButton button =
                new MockImageButtonBuilder()
                        .withDrawable(DEFAULT_FORWARD_BUTTON_DRAWABLE)
                        .withIsEnabled(false)
                        .build();
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withForwardButton(button).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.FORWARD_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentReloadButton_Level() {
        TabletCaptureStateTokenBuilder tabletTokenBuilder = new TabletCaptureStateTokenBuilder();
        ImageButton button =
                new MockImageButtonBuilder()
                        .withDrawable(DEFAULT_RELOAD_BUTTON_DRAWABLE)
                        .withLevel(5)
                        .build();
        TabletCaptureStateToken tabletToken = tabletTokenBuilder.withReloadButton(button).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.RELOAD_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentReloadButton_Enabled() {
        ImageButton button =
                new MockImageButtonBuilder()
                        .withDrawable(DEFAULT_RELOAD_BUTTON_DRAWABLE)
                        .withIsEnabled(false)
                        .build();
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withReloadButton(button).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.RELOAD_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentSecurityIcon() {
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withSecurityIcon(123).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.SECURITY_ICON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentVisibleUrlText() {
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder()
                        .withVisibleUrlText(new VisibleUrlText("foo", "bar"))
                        .build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.URL_TEXT,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentBookmarkButton_IconRes() {
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withBookmarkIconRes(123).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.BOOKMARK_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentBookmarkButton_Color() {
        ImageButton button = new MockImageButtonBuilder().withColor(Color.GREEN).build();
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withBookmarkButton(button).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.BOOKMARK_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentBookmarkButton_Enabled() {
        ImageButton button = new MockImageButtonBuilder().withIsEnabled(false).build();
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withBookmarkButton(button).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.BOOKMARK_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentOptionalButton_NullButton() {
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withOptionalButton(null).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.OPTIONAL_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentOptionalButton_NullDrawable() {
        ImageButton button = new MockImageButtonBuilder().withDrawable(null).build();
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withOptionalButton(button).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.OPTIONAL_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentOptionalButton_NullImageTintList() {
        ImageButton button =
                new MockImageButtonBuilder()
                        .withDrawable(DEFAULT_OPTIONAL_BUTTON_DRAWABLE)
                        .withHasImageTintList(false)
                        .build();
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withOptionalButton(button).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.OPTIONAL_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentTabCount() {
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withTabCount(10).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.TAB_COUNT,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentViewWidth() {
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withViewWidth(2000).build();
        Assert.assertEquals(
                ToolbarSnapshotDifference.LOCATION_BAR_WIDTH,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }
}
