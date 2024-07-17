// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import android.text.TextUtils;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.omnibox.R;

/**
 * Omnibox Actions are additional actions associated with Omnibox Matches. For more information,
 * please check on OmniboxAction class definition on native side.
 */
public abstract class OmniboxAction {
    /** Describes the ChipView decoration. */
    public static final class ChipIcon {
        public final @DrawableRes int iconRes;
        public final boolean tintWithTextColor;

        /**
         * @param iconRes The resource Id of the icon to be shown beside the text.
         * @param tintWithTextColor Whether to tint the icon using primary text color.
         */
        public ChipIcon(@DrawableRes int iconRes, boolean tintWithTextColor) {
            this.iconRes = iconRes;
            this.tintWithTextColor = tintWithTextColor;
        }
    }

    /** The default action icon. */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public static final ChipIcon DEFAULT_ICON =
            new ChipIcon(R.drawable.action_default, /* tintWithTextColor= */ false);

    /** ChipIcon instance specifying no icon should be shown. */
    protected static final ChipIcon NO_ICON =
            new ChipIcon(ChipView.INVALID_ICON_ID, /* tintWithTextColor= */ false);

    /** The type of an underlying action. */
    public final @OmniboxActionId int actionId;

    /** The string to present/announce to the user when the action is shown. */
    public final @NonNull String hint;

    /** The text to announce when the action chip is focused. */
    public final @NonNull String accessibilityHint;

    /** The icon to use to decorate the Action chip. */
    public final @NonNull ChipIcon icon;

    public final int primaryTextAppearance;

    /** The corresponding native instance, or 0 if the native instance is not available. */
    private long mNativeInstance;

    public OmniboxAction(
            @OmniboxActionId int actionId,
            long nativeInstance,
            @NonNull String hint,
            @NonNull String accessibilityHint,
            @NonNull ChipIcon icon,
            int primaryTextAppearance) {
        assert !TextUtils.isEmpty(hint);
        this.actionId = actionId;
        this.hint = hint;
        this.accessibilityHint = accessibilityHint;
        this.icon = icon;
        this.primaryTextAppearance = primaryTextAppearance;
        mNativeInstance = nativeInstance;
    }

    @CalledByNative
    @VisibleForTesting
    public void destroy() {
        mNativeInstance = 0;
    }

    /**
     * Report information about pedal being shown.
     *
     * @return true if information was recorded.
     */
    public boolean recordActionShown(int position, boolean executed) {
        if (mNativeInstance != 0L) {
            OmniboxActionJni.get().recordActionShown(mNativeInstance, position, executed);
            return true;
        }

        return false;
    }

    public long getNativeInstance() {
        return mNativeInstance;
    }

    /**
     * Execute the associated action.
     *
     * @param delegate delegate capable of routing and executing variety of action-specific tasks
     */
    public abstract void execute(@NonNull OmniboxActionDelegate delegate);

    @NativeMethods
    public interface Natives {
        /** Emit histograms related to the action. */
        void recordActionShown(long nativeOmniboxAction, int position, boolean executed);
    }
}
