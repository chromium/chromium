// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;

import org.chromium.chrome.browser.omnibox.action.OmniboxActionType;

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

    /** The type of an underlying action. */
    public final @OmniboxActionType int actionId;
    /** The string to present/announce to the user when the action is shown. */
    public final @NonNull String hint;

    public OmniboxAction(@OmniboxActionType int type, @NonNull String hint) {
        this.actionId = type;
        this.hint = hint;
    }

    /** Returns the icon to present beside the action chip. */
    public abstract @NonNull ChipIcon getIcon();
}
