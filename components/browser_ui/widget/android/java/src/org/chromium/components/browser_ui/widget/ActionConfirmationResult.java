// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

// The result of processing an action.
@IntDef({
    ActionConfirmationResult.IMMEDIATE_CONTINUE,
    ActionConfirmationResult.CONFIRMATION_POSITIVE,
    ActionConfirmationResult.CONFIRMATION_NEGATIVE
})
@Retention(RetentionPolicy.SOURCE)
public @interface ActionConfirmationResult {
    // Did not show any confirmation, the action should immediately continue. Resulting action
    // should likely be undoable.
    int IMMEDIATE_CONTINUE = 0;
    // Confirmation was received from the user to continue the action. Do not make resulting
    // action undoable.
    int CONFIRMATION_POSITIVE = 1;
    // The user wants to cancel the action.
    int CONFIRMATION_NEGATIVE = 2;
}
