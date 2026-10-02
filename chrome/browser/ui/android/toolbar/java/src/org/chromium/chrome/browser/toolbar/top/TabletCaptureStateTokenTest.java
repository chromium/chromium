// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageButton;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TabletCaptureStateToken}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabletCaptureStateTokenTest {
    private static final Drawable DEFAULT_HOME_BUTTON_DRAWABLE = mock(Drawable.class);
    private static final Drawable DEFAULT_BACKWARD_BUTTON_DRAWABLE = mock(Drawable.class);
    private static final Drawable DEFAULT_FORWARD_BUTTON_DRAWABLE = mock(Drawable.class);
    private static final Drawable DEFAULT_RELOAD_BUTTON_DRAWABLE = mock(Drawable.class);
    private static final Drawable DEFAULT_OPTIONAL_BUTTON_DRAWABLE = mock(Drawable.class);

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

    private static class ImageButtonBuilder {
        private final ImageButton mImageButton;
        private final ColorStateList mColorStateList;

        private Drawable mDrawable;
        private boolean mHasImageTintList = DEFAULT_HAS_IMAGE_TINT_LIST;
        private int mVisibility = DEFAULT_VISIBILITY;
        private boolean mIsEnabled = DEFAULT_IS_ENABLED;
        private int mLevel = DEFAULT_LEVEL;
        private int mColor = DEFAULT_COLOR;

        ImageButtonBuilder() {
            mImageButton = new ImageButton(ContextUtils.getApplicationContext());
            mColorStateList = mock(ColorStateList.class);
        }

        ImageButtonBuilder withDrawable(Drawable drawable) {
            mDrawable = drawable;
            return this;
        }

        ImageButtonBuilder withHasImageTintList(boolean hasDrawable) {
            mHasImageTintList = hasDrawable;
            return this;
        }

        ImageButtonBuilder withVisibility(int visibility) {
            mVisibility = visibility;
            return this;
        }

        ImageButtonBuilder withIsEnabled(boolean isEnabled) {
            mIsEnabled = isEnabled;
            return this;
        }

        ImageButtonBuilder withLevel(int level) {
            mLevel = level;
            return this;
        }

        ImageButtonBuilder withColor(int color) {
            mColor = color;
            return this;
        }

        ImageButton build() {
            if (mDrawable != null) {
                when(mDrawable.mutate()).thenReturn(mDrawable);
                when(mDrawable.getLevel()).thenReturn(mLevel);
            }
            when(mColorStateList.getDefaultColor()).thenReturn(mColor);
            mImageButton.setImageDrawable(mDrawable);
            mImageButton.setImageTintList(mHasImageTintList ? mColorStateList : null);
            mImageButton.setVisibility(mVisibility);
            mImageButton.setEnabled(mIsEnabled);
            return mImageButton;
        }
    }

    private static class TabletCaptureStateTokenBuilder {
        private ImageButton mHomeButton =
                new ImageButtonBuilder().withDrawable(DEFAULT_HOME_BUTTON_DRAWABLE).build();
        private ImageButton mBackwardButton =
                new ImageButtonBuilder().withDrawable(DEFAULT_BACKWARD_BUTTON_DRAWABLE).build();
        private ImageButton mForwardButton =
                new ImageButtonBuilder().withDrawable(DEFAULT_FORWARD_BUTTON_DRAWABLE).build();
        private ImageButton mReloadButton =
                new ImageButtonBuilder().withDrawable(DEFAULT_RELOAD_BUTTON_DRAWABLE).build();
        private @DrawableRes int mSecurityIcon = DEFAULT_SECURITY_ICON;
        private VisibleUrlText mVisibleUrlText = DEFAULT_VISIBLE_URL_TEXT;
        private ImageButton mBookmarkButton = new ImageButtonBuilder().build();
        private @DrawableRes int mBookmarkIconRes = DEFAULT_BOOKMARK_ICON;
        private ImageButton mOptionalButton =
                new ImageButtonBuilder().withDrawable(DEFAULT_OPTIONAL_BUTTON_DRAWABLE).build();
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
        assertEquals(
                ToolbarSnapshotDifference.NONE, tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentNull() {
        assertEquals(ToolbarSnapshotDifference.NULL, mDefaultTabletToken.getAnyDifference(null));
    }

    @Test
    public void testDifferentHomeButton() {
        ImageButton button =
                new ImageButtonBuilder()
                        .withDrawable(DEFAULT_HOME_BUTTON_DRAWABLE)
                        .withVisibility(View.INVISIBLE)
                        .build();
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withHomeButton(button).build();
        assertEquals(
                ToolbarSnapshotDifference.HOME_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentBackButton() {
        ImageButton button =
                new ImageButtonBuilder()
                        .withDrawable(DEFAULT_BACKWARD_BUTTON_DRAWABLE)
                        .withIsEnabled(false)
                        .build();
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withBackwardButton(button).build();
        assertEquals(
                ToolbarSnapshotDifference.BACK_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentForwardButton() {
        ImageButton button =
                new ImageButtonBuilder()
                        .withDrawable(DEFAULT_FORWARD_BUTTON_DRAWABLE)
                        .withIsEnabled(false)
                        .build();
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withForwardButton(button).build();
        assertEquals(
                ToolbarSnapshotDifference.FORWARD_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentReloadButton_Level() {
        TabletCaptureStateTokenBuilder tabletTokenBuilder = new TabletCaptureStateTokenBuilder();
        ImageButton button =
                new ImageButtonBuilder()
                        .withDrawable(DEFAULT_RELOAD_BUTTON_DRAWABLE)
                        .withLevel(5)
                        .build();
        TabletCaptureStateToken tabletToken = tabletTokenBuilder.withReloadButton(button).build();
        assertEquals(
                ToolbarSnapshotDifference.RELOAD_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentReloadButton_Enabled() {
        ImageButton button =
                new ImageButtonBuilder()
                        .withDrawable(DEFAULT_RELOAD_BUTTON_DRAWABLE)
                        .withIsEnabled(false)
                        .build();
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withReloadButton(button).build();
        assertEquals(
                ToolbarSnapshotDifference.RELOAD_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentSecurityIcon() {
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withSecurityIcon(123).build();
        assertEquals(
                ToolbarSnapshotDifference.SECURITY_ICON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentVisibleUrlText() {
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder()
                        .withVisibleUrlText(new VisibleUrlText("foo", "bar"))
                        .build();
        assertEquals(
                ToolbarSnapshotDifference.URL_TEXT,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentBookmarkButton_IconRes() {
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withBookmarkIconRes(123).build();
        assertEquals(
                ToolbarSnapshotDifference.BOOKMARK_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentBookmarkButton_Color() {
        ImageButton button = new ImageButtonBuilder().withColor(Color.GREEN).build();
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withBookmarkButton(button).build();
        assertEquals(
                ToolbarSnapshotDifference.BOOKMARK_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentBookmarkButton_Enabled() {
        ImageButton button = new ImageButtonBuilder().withIsEnabled(false).build();
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withBookmarkButton(button).build();
        assertEquals(
                ToolbarSnapshotDifference.BOOKMARK_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentOptionalButton_NullButton() {
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withOptionalButton(null).build();
        assertEquals(
                ToolbarSnapshotDifference.OPTIONAL_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentOptionalButton_NullDrawable() {
        ImageButton button = new ImageButtonBuilder().withDrawable(null).build();
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withOptionalButton(button).build();
        assertEquals(
                ToolbarSnapshotDifference.OPTIONAL_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentOptionalButton_NullImageTintList() {
        ImageButton button =
                new ImageButtonBuilder()
                        .withDrawable(DEFAULT_OPTIONAL_BUTTON_DRAWABLE)
                        .withHasImageTintList(false)
                        .build();
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withOptionalButton(button).build();
        assertEquals(
                ToolbarSnapshotDifference.OPTIONAL_BUTTON,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentTabCount() {
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withTabCount(10).build();
        assertEquals(
                ToolbarSnapshotDifference.TAB_COUNT,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }

    @Test
    public void testDifferentViewWidth() {
        TabletCaptureStateToken tabletToken =
                new TabletCaptureStateTokenBuilder().withViewWidth(2000).build();
        assertEquals(
                ToolbarSnapshotDifference.LOCATION_BAR_WIDTH,
                tabletToken.getAnyDifference(mDefaultTabletToken));
    }
}
