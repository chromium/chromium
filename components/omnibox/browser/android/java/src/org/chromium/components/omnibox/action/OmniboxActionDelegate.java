// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

/**
 * An interface for handling interactions for Omnibox Action Chips.
 * TODO(crbug/1418077): repurpose as a OmniboxActionFactory.
 */
public interface OmniboxActionDelegate {
    /**
     * Call this method when the pedal is clicked.
     *
     * @param action the {@link OmniboxAction} whose action we want to execute.
     */
    void execute(OmniboxAction action);
}
