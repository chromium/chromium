// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import android.text.TextUtils;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.omnibox.R;

/**
 * Omnibox Actions are additional actions associated with Omnibox Matches. For more information,
 * please check on OmniboxAction class definition on native side.
 */
@NullMarked
public abstract class OmniboxAction {
    /** Describes the ChipView decoration. */
    public static final class ActionIcon {
        public final @DrawableRes int chipIconRes;
        public final boolean tintWithTextColor;

        public final @DrawableRes int buttonIconRes;
        public final @DrawableRes int incognitoButtonIconRes;

        /**
         * @param chipIconRes The resource Id of the icon to be shown beside the text.
         * @param buttonIconRes The resource Id of the icon to be shown for the action button.
         * @param incognitoButtonIconRes The resource Id of the icon to be shown for the action
         *     button in incognito.
         * @param tintWithTextColor Whether to tint the icon using primary text color.
         */
        public ActionIcon(
                @DrawableRes int chipIconRes,
                @DrawableRes int buttonIconRes,
                @DrawableRes int incognitoButtonIconRes,
                boolean tintWithTextColor) {
            this.chipIconRes = chipIconRes;
            this.buttonIconRes = buttonIconRes;
            this.incognitoButtonIconRes = incognitoButtonIconRes;
            this.tintWithTextColor = tintWithTextColor;
        }

        /**
         * @param iconRes The resource Id of the icon to be shown beside the text.
         * @param tintWithTextColor Whether to tint the icon using primary text color.
         */
        public ActionIcon(@DrawableRes int iconRes, boolean tintWithTextColor) {
            this(iconRes, iconRes, iconRes, tintWithTextColor);
        }
    }

    /** The default action icon. */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public static final ActionIcon DEFAULT_ICON =
            new ActionIcon(R.drawable.action_default, /* tintWithTextColor= */ false);

    /** ActionIcon instance specifying no icon should be shown. */
    protected static final ActionIcon NO_ICON =
            new ActionIcon(ChipView.INVALID_ICON_ID, /* tintWithTextColor= */ false);

    /** The type of an underlying action. */
    public final @OmniboxActionId int actionId;

    /** The string to present/announce to the user when the action is shown. */
    public final String hint;

    /** The text to announce when the action chip is focused. */
    public final String accessibilityHint;

    /** The icon to use to decorate the Action chip. */
    public final ActionIcon icon;

    public final int primaryTextAppearance;

    /** Whether to show it as action button. */
    public final boolean showAsActionButton;

    /** The window open disposition. */
    public int disposition;

    /** The corresponding native instance, or 0 if the native instance is not available. */
    private long mNativeInstance;

    public OmniboxAction(
            @OmniboxActionId int actionId,
            long nativeInstance,
            String hint,
            String accessibilityHint,
            ActionIcon icon,
            int primaryTextAppearance,
            boolean showAsActionButton,
            int disposition) {
        assert !TextUtils.isEmpty(hint);
        this.actionId = actionId;
        this.hint = hint;
        this.accessibilityHint = accessibilityHint;
        this.icon = icon;
        this.primaryTextAppearance = primaryTextAppearance;
        this.showAsActionButton = showAsActionButton;
        this.disposition = disposition;
        mNativeInstance = nativeInstance;
    }

    @CalledByNative
    @VisibleForTesting
    public void destroy() {
        mNativeInstance = 0;
    }

    public long getNativeInstance() {
        return mNativeInstance;
    }

    /**
     * Execute the associated action.
     *
     * @param delegate delegate capable of routing and executing variety of action-specific tasks
     */
    public abstract void execute(OmniboxActionDelegate delegate);
}
