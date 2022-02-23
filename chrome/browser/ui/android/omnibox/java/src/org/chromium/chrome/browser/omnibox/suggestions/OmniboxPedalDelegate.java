// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.omnibox.action.OmniboxPedalType;
import org.chromium.chrome.browser.omnibox.suggestions.pedal.PedalSuggestionViewProperties.PedalIcon;

/**
 * An interface for handling interactions for Omnibox Pedals.
 */
public interface OmniboxPedalDelegate {
    /**
     * Call this method when the pedal is clicked.
     *
     * @param omniboxActionType the {@link OmniboxActionType} related to the clicked pedal.
     */
    void executeAction(@OmniboxPedalType int omniboxActionType);

    /**
     * Call this method when request the pedal's icon.
     *
     * @param omniboxActionType the {@link OmniboxActionType} for the request pedal.
     * @return The icon's information.
     */
    @NonNull
    PedalIcon getPedalIcon(@OmniboxPedalType int omniboxActionType);
}