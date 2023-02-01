// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.omnibox.suggestions.pedal.PedalViewProperties.PedalIcon;
import org.chromium.components.omnibox.action.OmniboxPedal;

/**
 * An interface for handling interactions for Omnibox Pedals and actions.
 */
public interface OmniboxPedalDelegate {
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
    PedalIcon getIcon(OmniboxPedal omniboxPedal);
}