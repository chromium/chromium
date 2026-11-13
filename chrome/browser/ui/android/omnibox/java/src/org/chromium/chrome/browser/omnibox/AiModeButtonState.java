// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * How the stateful AI Mode button (the activation chip) is presented. The state is never stored as
 * a source of truth; it is derived from the autocomplete request type and whether there is content
 * to submit. See {@link LocationBarMediator#computeAiModeButtonState()}.
 */
@IntDef({
    AiModeButtonState.HIDDEN,
    AiModeButtonState.VISIBLE_INACTIVE,
    AiModeButtonState.VISIBLE_ACTIVE,
    AiModeButtonState.VISIBLE_SEND
})
@Retention(RetentionPolicy.SOURCE)
@NullMarked
@interface AiModeButtonState {
    /** The button is not shown. */
    int HIDDEN = 0;

    /** Conventional search; clicking the button enters AI Mode. */
    int VISIBLE_INACTIVE = 1;

    /** AI Mode with nothing to submit; clicking the button returns to conventional search. */
    int VISIBLE_ACTIVE = 2;

    /** AI Mode with text or attachments; clicking the button submits the query. */
    int VISIBLE_SEND = 3;
}
