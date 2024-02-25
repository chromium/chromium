// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;

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
    private final int mOptionalButtonDataHashCode;
    private final @VisualState int mVisualState;
    private final VisibleUrlText mVisibleUrlText;
    private final @DrawableRes int mSecurityIcon;
    private final ColorStateList mHomeButtonColorStateList;
    private final boolean mHomeButtonIsVisible;
    private final boolean mIsShowingUpdateBadgeDuringLastCapture;
    private final boolean mIsPaintPreview;
    private final int mUnfocusedLocationBarLayoutWidth;

    public PhoneCaptureStateToken(
            @ColorInt int tint,
            int tabCount,
            ButtonData optionalButtonData,
            @VisualState int visualState,
            VisibleUrlText visibleUrlText,
            @DrawableRes int securityIcon,
            ColorStateList homeButtonColorStateList,
            boolean homeButtonIsVisible,
            boolean isShowingUpdateBadgeDuringLastCapture,
            boolean isPaintPreview,
            float progress,
            int unfocusedLocationBarLayoutWidth) {
        mTint = tint;
        mTabCount = tabCount;
        mOptionalButtonDataHashCode = Objects.hashCode(optionalButtonData);
        mVisualState = visualState;
        mVisibleUrlText = visibleUrlText;
        mSecurityIcon = securityIcon;
        mHomeButtonColorStateList = homeButtonColorStateList;
        mHomeButtonIsVisible = homeButtonIsVisible;
        mIsShowingUpdateBadgeDuringLastCapture = isShowingUpdateBadgeDuringLastCapture;
        mIsPaintPreview = isPaintPreview;
        // Progress is not currently used for comparing snapshot states. It isn't part of the bitmap
        // capture anyway.
        mUnfocusedLocationBarLayoutWidth = unfocusedLocationBarLayoutWidth;
    }

    /**
     * Returns the first difference found between the two snapshots. If no difference is found, then
     * {@link ToolbarSnapshotDifference.UNKNOWN} is returned.
     *
     * @param current The current snapshot.
     * @param next The new snapshot to compare against.
     * @return The difference.
     */
    public static @ToolbarSnapshotDifference int getAnyDifference(
            PhoneCaptureStateToken current, PhoneCaptureStateToken next) {
        assert next != null;
        if (current == null) {
            return ToolbarSnapshotDifference.NULL;
        } else if (current.mTint != next.mTint) {
            return ToolbarSnapshotDifference.TINT;
        } else if (current.mTabCount != next.mTabCount) {
            return ToolbarSnapshotDifference.TAB_COUNT;
        } else if (current.mOptionalButtonDataHashCode != next.mOptionalButtonDataHashCode) {
            return ToolbarSnapshotDifference.OPTIONAL_BUTTON;
        } else if (current.mVisualState != next.mVisualState) {
            return ToolbarSnapshotDifference.VISUAL_STATE;
        } else if (current.mSecurityIcon != next.mSecurityIcon) {
            return ToolbarSnapshotDifference.SECURITY_ICON;
        } else if (current.mIsShowingUpdateBadgeDuringLastCapture
                != next.mIsShowingUpdateBadgeDuringLastCapture) {
            return ToolbarSnapshotDifference.SHOWING_UPDATE_BADGE;
        } else if (current.mIsPaintPreview != next.mIsPaintPreview) {
            return ToolbarSnapshotDifference.PAINT_PREVIEW;
        } else if (current.mUnfocusedLocationBarLayoutWidth
                != next.mUnfocusedLocationBarLayoutWidth) {
            return ToolbarSnapshotDifference.LOCATION_BAR_WIDTH;
        } else if (!VisibleUrlText.isVisuallyEquivalent(
                current.mVisibleUrlText, next.mVisibleUrlText)) {
            return ToolbarSnapshotDifference.URL_TEXT;
        } else if (current.mHomeButtonColorStateList.getDefaultColor()
                        != next.mHomeButtonColorStateList.getDefaultColor()
                || current.mHomeButtonIsVisible != next.mHomeButtonIsVisible) {
            // While there's more to the ColorStateList than just the default color, there's no
            // great way to check for equality. Currently default colors should be sufficient for
            // detecting changes to the toolbar.
            return ToolbarSnapshotDifference.HOME_BUTTON;
        }
        return ToolbarSnapshotDifference.NONE;
    }

    @ColorInt
    int getTint() {
        return mTint;
    }

    int getTabCount() {
        return mTabCount;
    }
}
