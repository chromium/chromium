// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;

import org.chromium.components.omnibox.action.OmniboxPedal;

/**
 * An interface for handling interactions for Omnibox Action Chips.
 */
public interface ActionChipsDelegate {
    /**
     * Describes the ChipView decoration.
     */
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

    /**
     * Call this method when the pedal is clicked.
     *
     * @param omniboxPedal the {@link OmniboxPedal} whose action we want to execute.
     */
    void execute(OmniboxPedal omniboxPedal);

    /**
     * Call this method to request the pedal's icon.
     *
     * @param omniboxPedal the {@link OmniboxPedal} whose icon we want.
     * @return The icon's information.
     */
    @NonNull
    ChipIcon getIcon(OmniboxPedal omniboxPedal);
}
