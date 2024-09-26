// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.graphics.drawable.Drawable;
import android.widget.ImageButton;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.Objects;

/**
 * If {@link TabletCaptureStateToken#getAnyDifference} returns NONE, the bitmap that would be
 * produced by capturing is identical to the bitmap corresponding to the given token. This is used
 * to avoid re-capturing unnecessarily.
 */
class TabletCaptureStateToken {
    private final @Nullable ButtonCaptureStateToken mHomeButtonToken;
    private final @Nullable ButtonCaptureStateToken mBackwardButtonToken;
    private final @Nullable ButtonCaptureStateToken mForwardButtonToken;
    private final @Nullable ButtonCaptureStateToken mReloadButtonToken;
    // Security button is behind abstractions that only more easily just expose the res.
    private final @DrawableRes int mSecurityIcon;
    private final @Nullable VisibleUrlText mVisibleUrlText;
    private final @Nullable ButtonCaptureStateToken mBookmarkButtonToken;
    private final @Nullable ButtonCaptureStateToken mOptionalButtonToken;
    // Tab count is a better proxy for this visual state.
    private final int mTabCount;
    private final int mViewWidth;

    private static ButtonCaptureStateToken buttonTokenUseDrawableInstance(
            @Nullable ImageButton imageButton) {
        return imageButton == null
                ? null
                : new DrawableInstanceButtonCaptureStateToken(imageButton);
    }

    private static ButtonCaptureStateToken buttonTokenWithDrawableRes(
            @Nullable ImageButton imageButton, @DrawableRes int iconRes) {
        return imageButton == null
                ? null
                : new DrawableResButtonCaptureStateToken(imageButton, iconRes);
    }

    /**
     * Superset of all the fields that might be changing on buttons that could affect the way they
     * are drawn. Some of these fields never change for some of the buttons. Subclassed to have a
     * way of comparing the actual image.
     */
    private abstract static class ButtonCaptureStateToken {
        private final int mVisibility;
        private final boolean mIsEnabled;
        private final int mLevel;
        private final @ColorInt int mImageTint;

        private ButtonCaptureStateToken(@NonNull ImageButton imageButton) {
            mVisibility = imageButton.getVisibility();
            mIsEnabled = imageButton.isEnabled();
            mLevel = imageButton.getDrawable() == null ? 0 : imageButton.getDrawable().getLevel();
            mImageTint =
                    imageButton.getImageTintList() == null
                            ? 0
                            : imageButton.getImageTintList().getDefaultColor();
        }

        @Override
        public boolean equals(@Nullable Object o) {
            if (this == o) return true;
            if (!(o instanceof ButtonCaptureStateToken)) return false;
            ButtonCaptureStateToken that = (ButtonCaptureStateToken) o;
            return mVisibility == that.mVisibility
                    && mIsEnabled == that.mIsEnabled
                    && mLevel == that.mLevel
                    && mImageTint == that.mImageTint;
        }

        @Override
        public int hashCode() {
            return System.identityHashCode(this)
                    + Integer.hashCode(mVisibility)
                    + Boolean.hashCode(mIsEnabled)
                    + Integer.hashCode(mLevel)
                    + Integer.hashCode(mImageTint);
        }
    }

    private static class DrawableInstanceButtonCaptureStateToken extends ButtonCaptureStateToken {
        private final Drawable mImageDrawable;

        private DrawableInstanceButtonCaptureStateToken(@NonNull ImageButton imageButton) {
            super(imageButton);
            mImageDrawable = imageButton.getDrawable();
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof DrawableInstanceButtonCaptureStateToken)) return false;
            DrawableInstanceButtonCaptureStateToken that =
                    (DrawableInstanceButtonCaptureStateToken) o;
            return Objects.equals(mImageDrawable, that.mImageDrawable) && super.equals(o);
        }

        @Override
        public int hashCode() {
            return super.hashCode() + Objects.hashCode(mImageDrawable);
        }
    }

    private static class DrawableResButtonCaptureStateToken extends ButtonCaptureStateToken {
        private final @DrawableRes int mDrawableRes;

        private DrawableResButtonCaptureStateToken(
                @NonNull ImageButton imageButton, @DrawableRes int drawableRes) {
            super(imageButton);
            mDrawableRes = drawableRes;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof DrawableResButtonCaptureStateToken)) return false;
            DrawableResButtonCaptureStateToken that = (DrawableResButtonCaptureStateToken) o;
            return mDrawableRes == that.mDrawableRes && super.equals(o);
        }

        @Override
        public int hashCode() {
            return super.hashCode() + Integer.hashCode(mDrawableRes);
        }
    }

    public TabletCaptureStateToken(
            @Nullable ImageButton homeButton,
            @Nullable ImageButton backwardButton,
            @Nullable ImageButton forwardButton,
            @Nullable ImageButton reloadButton,
            @DrawableRes int securityIcon,
            @Nullable VisibleUrlText visibleUrlText,
            @Nullable ImageButton bookmarkButton,
            @DrawableRes int bookmarkButtonImageRes,
            @Nullable ImageButton optionalButton,
            int tabCount,
            int viewWidth) {
        mHomeButtonToken = buttonTokenUseDrawableInstance(homeButton);
        mBackwardButtonToken = buttonTokenUseDrawableInstance(backwardButton);
        mForwardButtonToken = buttonTokenUseDrawableInstance(forwardButton);
        mReloadButtonToken = buttonTokenUseDrawableInstance(reloadButton);
        mSecurityIcon = securityIcon;
        mVisibleUrlText = visibleUrlText;
        mBookmarkButtonToken = buttonTokenWithDrawableRes(bookmarkButton, bookmarkButtonImageRes);
        mOptionalButtonToken = buttonTokenUseDrawableInstance(optionalButton);
        mTabCount = tabCount;
        mViewWidth = viewWidth;
    }

    public @ToolbarSnapshotDifference int getAnyDifference(TabletCaptureStateToken that) {
        if (that == null) {
            return ToolbarSnapshotDifference.NULL;
        } else if (!Objects.equals(mHomeButtonToken, that.mHomeButtonToken)) {
            return ToolbarSnapshotDifference.HOME_BUTTON;
        } else if (!Objects.equals(mBackwardButtonToken, that.mBackwardButtonToken)) {
            return ToolbarSnapshotDifference.BACK_BUTTON;
        } else if (!Objects.equals(mForwardButtonToken, that.mForwardButtonToken)) {
            return ToolbarSnapshotDifference.FORWARD_BUTTON;
        } else if (!Objects.equals(mReloadButtonToken, that.mReloadButtonToken)) {
            return ToolbarSnapshotDifference.RELOAD_BUTTON;
        } else if (mSecurityIcon != that.mSecurityIcon) {
            return ToolbarSnapshotDifference.SECURITY_ICON;
        } else if (!Objects.equals(mVisibleUrlText, that.mVisibleUrlText)) {
            return ToolbarSnapshotDifference.URL_TEXT;
        } else if (!Objects.equals(mBookmarkButtonToken, that.mBookmarkButtonToken)) {
            return ToolbarSnapshotDifference.BOOKMARK_BUTTON;
        } else if (!Objects.equals(mOptionalButtonToken, that.mOptionalButtonToken)) {
            return ToolbarSnapshotDifference.OPTIONAL_BUTTON;
        } else if (mTabCount != that.mTabCount) {
            return ToolbarSnapshotDifference.TAB_COUNT;
        } else if (mViewWidth != that.mViewWidth) {
            return ToolbarSnapshotDifference.LOCATION_BAR_WIDTH;
        } else {
            return ToolbarSnapshotDifference.NONE;
        }
    }
}
