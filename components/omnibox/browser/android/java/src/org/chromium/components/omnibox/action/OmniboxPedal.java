// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.omnibox.action.OmniboxActionType;
import org.chromium.chrome.browser.omnibox.action.OmniboxPedalType;
import org.chromium.components.browser_ui.styles.R;

/**
 * Omnibox Actions are additional actions associated with Omnibox Matches. For more information,
 * please check on OmniboxAction class definition on native side.
 */
public class OmniboxPedal extends OmniboxAction {
    /** The type of the underlying pedal. */
    public final @OmniboxPedalType int pedalId;

    @CalledByNative
    public OmniboxPedal(@NonNull String hint, @OmniboxPedalType int pedalId) {
        super(OmniboxActionType.PEDAL, hint);
        this.pedalId = pedalId;
    }

    @Override
    public @NonNull ChipIcon getIcon() {
        if (pedalId == OmniboxPedalType.PLAY_CHROME_DINO_GAME) {
            return new ChipIcon(R.drawable.ic_dino, /*tintWithTextColor=*/true);
        }
        return new ChipIcon(R.drawable.fre_product_logo, /*tintWithTextColor=*/false);
    }

    /**
     * Cast supplied OmniboxAction to OmniboxPedal.
     * Requires the supplied input to be a valid instance of an OmniboxPedal whose
     * actionId is the PEDAL.
     */
    public static @NonNull OmniboxPedal from(@NonNull OmniboxAction action) {
        assert action != null;
        assert action.actionId == OmniboxActionType.PEDAL;
        assert action instanceof OmniboxPedal;
        return (OmniboxPedal) action;
    }
}
