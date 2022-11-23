// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.res.ColorStateList;
import android.text.TextUtils;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone.VisualState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/**
 * A collections of stored information about the toolbar's current state. Allows checking old states
 * against new states, to infer if anything important has changed. Especially useful when deciding
 * if a new bitmap capture is warranted.
 */
public class ToolbarSnapshotState {
    /**
     * Reasons that two snapshots are different. Treat this list as append only and keep it in sync
     * with ToolbarSnapshotDifference in enums.xml, as well as the proto in
     * chrome_track_event.proto.
     **/
    @IntDef({ToolbarSnapshotDifference.NONE, ToolbarSnapshotDifference.NULL,
            ToolbarSnapshotDifference.TINT, ToolbarSnapshotDifference.TAB_COUNT,
            ToolbarSnapshotDifference.OPTIONAL_BUTTON_DATA, ToolbarSnapshotDifference.VISUAL_STATE,
            ToolbarSnapshotDifference.SECURITY_ICON, ToolbarSnapshotDifference.SHOWING_UPDATE_BADGE,
            ToolbarSnapshotDifference.PAINT_PREVIEW, ToolbarSnapshotDifference.PROGRESS,
            ToolbarSnapshotDifference.LOCATION_BAR_WIDTH, ToolbarSnapshotDifference.URL_TEXT,
            ToolbarSnapshotDifference.HOME_BUTTON_COLOR, ToolbarSnapshotDifference.TITLE_TEXT,
            ToolbarSnapshotDifference.CCT_ANIMATION, ToolbarSnapshotDifference.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ToolbarSnapshotDifference {
        int NONE = 0;
        int NULL = 1;
        int TINT = 2;
        int TAB_COUNT = 3;
        int OPTIONAL_BUTTON_DATA = 4;
        int VISUAL_STATE = 5;
        int SECURITY_ICON = 6;
        int SHOWING_UPDATE_BADGE = 7;
        int PAINT_PREVIEW = 8;
        int PROGRESS = 9;
        int LOCATION_BAR_WIDTH = 10;
        int URL_TEXT = 11;
        int HOME_BUTTON_COLOR = 12;
        int TITLE_TEXT = 13;
        int CCT_ANIMATION = 14;
        int NUM_ENTRIES = 15;
    }

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

    public ToolbarSnapshotState(@ColorInt int tint, int tabCount, ButtonData optionalButtonData,
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
    public @ToolbarSnapshotDifference int getAnyDifference(ToolbarSnapshotState that) {
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

    private boolean isVisibleUrlTextSame(ToolbarSnapshotState that) {
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
