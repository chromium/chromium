// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.res.ColorStateList;
import android.text.TextUtils;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone.VisualState;

import java.util.Objects;

/**
 * A collections of stored information about the toolbar's current state. Allows checking old states
 * against new states, to infer if anything important has changed. Especially useful when deciding
 * if a new bitmap capture is warranted.
 */
class PhoneCaptureStateToken {
    private final @ColorInt int mTint;
    private final int mTabCount;
    private final ButtonData mOptionalButtonData;
    private final @VisualState int mVisualState;
    private final String mUrlText;
    @Nullable
    private final CharSequence mVisibleTextPrefixHint;
    private final @DrawableRes int mSecurityIcon;
    private final ColorStateList mColorStateList;
    private final boolean mIsShowingUpdateBadgeDuringLastCapture;
    private final boolean mIsPaintPreview;
    private final int mUnfocusedLocationBarLayoutWidth;

    public PhoneCaptureStateToken(@ColorInt int tint, int tabCount, ButtonData optionalButtonData,
            @VisualState int visualState, String urlText,
            @Nullable CharSequence visibleTextPrefixHint, @DrawableRes int securityIcon,
            ColorStateList colorStateList, boolean isShowingUpdateBadgeDuringLastCapture,
            boolean isPaintPreview, float progress, int unfocusedLocationBarLayoutWidth) {
        mTint = tint;
        mTabCount = tabCount;
        mOptionalButtonData = optionalButtonData;
        mVisualState = visualState;
        mUrlText = urlText;
        mVisibleTextPrefixHint = visibleTextPrefixHint;
        if (visibleTextPrefixHint != null) {
            assert isValidVisibleTextPrefixHint(urlText, visibleTextPrefixHint);
        }
        mSecurityIcon = securityIcon;
        mColorStateList = colorStateList;
        mIsShowingUpdateBadgeDuringLastCapture = isShowingUpdateBadgeDuringLastCapture;
        mIsPaintPreview = isPaintPreview;
        // Progress is not currently used for comparing snapshot states. It isn't part of the bitmap
        // capture anyway.
        mUnfocusedLocationBarLayoutWidth = unfocusedLocationBarLayoutWidth;
    }

    /**
     * Returns the first difference found between the two snapshots. If no difference is found, then
     * {@link ToolbarSnapshotDifference.UNKNOWN} is returned.
     * @param that The other snapshot to compare against.
     * @return The difference.
     */
    public @ToolbarSnapshotDifference int getAnyDifference(PhoneCaptureStateToken that) {
        if (that == null) {
            return ToolbarSnapshotDifference.NULL;
        } else if (mTint != that.mTint) {
            return ToolbarSnapshotDifference.TINT;
        } else if (mTabCount != that.mTabCount) {
            return ToolbarSnapshotDifference.TAB_COUNT;
        } else if (!Objects.equals(mOptionalButtonData, that.mOptionalButtonData)) {
            return ToolbarSnapshotDifference.OPTIONAL_BUTTON_DATA;
        } else if (mVisualState != that.mVisualState) {
            return ToolbarSnapshotDifference.VISUAL_STATE;
        } else if (mSecurityIcon != that.mSecurityIcon) {
            return ToolbarSnapshotDifference.SECURITY_ICON;
        } else if (mIsShowingUpdateBadgeDuringLastCapture
                != that.mIsShowingUpdateBadgeDuringLastCapture) {
            return ToolbarSnapshotDifference.SHOWING_UPDATE_BADGE;
        } else if (mIsPaintPreview != that.mIsPaintPreview) {
            return ToolbarSnapshotDifference.PAINT_PREVIEW;
        } else if (mUnfocusedLocationBarLayoutWidth != that.mUnfocusedLocationBarLayoutWidth) {
            return ToolbarSnapshotDifference.LOCATION_BAR_WIDTH;
        } else if (!isVisibleUrlTextSame(that)) {
            return ToolbarSnapshotDifference.URL_TEXT;
        } else if (mColorStateList.getDefaultColor() != that.mColorStateList.getDefaultColor()) {
            // While there's more to the ColorStateList than just the default color, there's no
            // great way to check for equality. Currently default colors should be sufficient for
            // detecting changes to the toolbar.
            return ToolbarSnapshotDifference.HOME_BUTTON_COLOR;
        }
        return ToolbarSnapshotDifference.NONE;
    }

    private boolean isVisibleUrlTextSame(PhoneCaptureStateToken that) {
        if (mVisibleTextPrefixHint != null
                && TextUtils.equals(mVisibleTextPrefixHint, that.mVisibleTextPrefixHint)) {
            return true;
        }
        return TextUtils.equals(mUrlText, that.mUrlText);
    }

    @ColorInt
    int getTint() {
        return mTint;
    }

    int getTabCount() {
        return mTabCount;
    }

    /**
     * Determines the validity of the hint text given the passed in full text.
     * @param fullText The full text that should start with the hint.
     * @param hintText The hint text to be checked.
     * @return Whether the full text starts with the specified hint text.
     */
    static boolean isValidVisibleTextPrefixHint(CharSequence fullText, CharSequence hintText) {
        if (fullText == null || TextUtils.isEmpty(hintText)) return false;
        if (hintText.length() > fullText.length()) return false;
        return TextUtils.indexOf(fullText, hintText, 0, hintText.length()) == 0;
    }
}
