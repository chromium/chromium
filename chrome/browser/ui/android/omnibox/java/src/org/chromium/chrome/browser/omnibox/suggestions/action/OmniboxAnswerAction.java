// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import androidx.annotation.NonNull;

import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;

/** Omnibox action for showing an Action associated with an Answer. */
public class OmniboxAnswerAction extends OmniboxAction {

    public OmniboxAnswerAction(
            long nativeInstance, @NonNull String hint, @NonNull String accessibilityHint) {
        super(OmniboxActionId.ANSWER_ACTION, nativeInstance, hint, accessibilityHint, NO_ICON);
    }

    @Override
    public void execute(@NonNull OmniboxActionDelegate delegate) {
        // Not yet implemented.
    }
}
