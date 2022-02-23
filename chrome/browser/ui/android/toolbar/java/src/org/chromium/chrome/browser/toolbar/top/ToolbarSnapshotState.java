// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.res.ColorStateList;

import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone.VisualState;

import java.util.Objects;

class ToolbarSnapshotState {
    private final int mTint;
    private final int mTabCount;
    private final ButtonData mOptionalButtonData;
    private final @VisualState int mVisualState;
    private String mUrlText = "";
    private final int mSecurityIcon;
    private final ColorStateList mColorStateList;
    private final boolean mIsShowingUpdateBadgeDuringLastCapture;
    private final boolean mIsPaintPreview;
    private final float mProgress;
    private final int mUnfocusedLocationBarLayoutWidth;

    public ToolbarSnapshotState(int tint, int tabCount, ButtonData optionalButtonData,
            @VisualState int visualState, String urlText, int securityIcon,
            ColorStateList colorStateList, boolean isShowingUpdateBadgeDuringLastCapture,
            boolean isPaintPreview, float progress, int unfocusedLocationBarLayoutWidth) {
        mTint = tint;
        mTabCount = tabCount;
        mOptionalButtonData = optionalButtonData;
        mVisualState = visualState;
        mUrlText = urlText;
        mSecurityIcon = securityIcon;
        mColorStateList = colorStateList;
        mIsShowingUpdateBadgeDuringLastCapture = isShowingUpdateBadgeDuringLastCapture;
        mIsPaintPreview = isPaintPreview;
        mProgress = progress;
        mUnfocusedLocationBarLayoutWidth = unfocusedLocationBarLayoutWidth;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof ToolbarSnapshotState)) {
            return false;
        }
        ToolbarSnapshotState that = (ToolbarSnapshotState) o;
        return mTint == that.mTint && mTabCount == that.mTabCount
                && Objects.equals(mOptionalButtonData, that.mOptionalButtonData)
                && mVisualState == that.mVisualState && mSecurityIcon == that.mSecurityIcon
                && mIsShowingUpdateBadgeDuringLastCapture
                == that.mIsShowingUpdateBadgeDuringLastCapture
                && mIsPaintPreview == that.mIsPaintPreview
                && Float.compare(mProgress, that.mProgress) == 0
                && mUnfocusedLocationBarLayoutWidth == that.mUnfocusedLocationBarLayoutWidth
                && Objects.equals(mUrlText, that.mUrlText)
                && Objects.equals(mColorStateList, that.mColorStateList);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mTint, mTabCount, mOptionalButtonData, mVisualState, mUrlText,
                mSecurityIcon, mColorStateList, mIsShowingUpdateBadgeDuringLastCapture,
                mIsPaintPreview, mProgress, mUnfocusedLocationBarLayoutWidth);
    }

    int getTint() {
        return mTint;
    }

    int getTabCount() {
        return mTabCount;
    }
}
