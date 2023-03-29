// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.components.omnibox.action.OmniboxAction;

/**
 * An interface for handling interactions for Omnibox Action Chips.
 */
public interface ActionChipsDelegate {
    /**
     * Call this method when the pedal is clicked.
     *
     * @param action the {@link OmniboxAction} whose action we want to execute.
     */
    void execute(OmniboxAction action);
}
