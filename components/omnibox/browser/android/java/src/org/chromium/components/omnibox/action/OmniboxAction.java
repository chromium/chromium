// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import android.text.TextUtils;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

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
    @VisibleForTesting
    public static final ChipIcon DEFAULT_ICON =
            new ChipIcon(R.drawable.action_default, /*tintWithTextColor=*/false);
    /** The type of an underlying action. */
    public final @OmniboxActionId int actionId;
    /** The string to present/announce to the user when the action is shown. */
    public final @NonNull String hint;
    /** The icon to use to decorate the Action chip. */
    public final @NonNull ChipIcon icon;

    public OmniboxAction(
            @OmniboxActionId int actionId, @NonNull String hint, @Nullable ChipIcon icon) {
        assert !TextUtils.isEmpty(hint);
        this.actionId = actionId;
        this.hint = hint;
        this.icon = icon != null ? icon : DEFAULT_ICON;
    }

    /**
     * Execute the associated action.
     *
     * @param delegate delegate capable of routing and executing variety of action-specific tasks
     */
    public abstract void execute(@NonNull OmniboxActionDelegate delegate);
}
