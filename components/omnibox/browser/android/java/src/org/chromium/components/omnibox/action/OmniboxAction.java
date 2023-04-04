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
public class OmniboxAction {
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
    static final ChipIcon DEFAULT_ICON =
            new ChipIcon(R.drawable.action_default, /*tintWithTextColor=*/false);
    /** The type of an underlying action. */
    public final @OmniboxActionType int actionId;
    /** The string to present/announce to the user when the action is shown. */
    public final @NonNull String hint;
    /** The icon to use to decorate the Action chip. */
    public final @NonNull ChipIcon icon;

    public OmniboxAction(
            @OmniboxActionType int type, @NonNull String hint, @Nullable ChipIcon icon) {
        assert !TextUtils.isEmpty(hint);
        this.actionId = type;
        this.hint = hint;
        this.icon = icon != null ? icon : DEFAULT_ICON;
    }
}
