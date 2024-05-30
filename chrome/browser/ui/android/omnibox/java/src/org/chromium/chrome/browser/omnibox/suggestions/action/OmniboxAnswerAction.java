// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import androidx.annotation.NonNull;

import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;
import org.chromium.url.GURL;

/** Omnibox action for showing an Action associated with an Answer. */
public class OmniboxAnswerAction extends OmniboxAction {

    @NonNull public final GURL destinationUrl;

    /**
     * Construct a new OmniboxAnswerAction.
     *
     * @param nativeInstance Pointer to native instance of the object.
     * @param hint Text that should be displayed in the associated action chip.
     * @param accessibilityHint Text for screen reader to read when focusing action chip
     * @param destinationUrl The URL of the SRP to navigate to when the action is executed.
     */
    public OmniboxAnswerAction(
            long nativeInstance,
            @NonNull String hint,
            @NonNull String accessibilityHint,
            @NonNull GURL destinationUrl) {
        super(OmniboxActionId.ANSWER_ACTION, nativeInstance, hint, accessibilityHint, NO_ICON);
        this.destinationUrl = destinationUrl;
    }

    @Override
    public void execute(@NonNull OmniboxActionDelegate delegate) {
        // Not yet implemented.
    }
}
