// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.chrome.browser.omnibox.action.OmniboxPedalType;

/**
 * An interface for handling click event on Omnibox Pedals.
 */
public interface OmniboxPedalDelegate {
    /**
     * Call this method when the pedal is clicked.
     *
     * @param omniboxActionType the {@link OmniboxActionType} related to the clicked pedal.
     */
    void executeAction(@OmniboxPedalType int omniboxActionType);
}