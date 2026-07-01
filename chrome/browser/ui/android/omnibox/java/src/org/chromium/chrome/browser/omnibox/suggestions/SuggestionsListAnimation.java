// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.build.annotations.NullMarked;

@NullMarked
public interface SuggestionsListAnimation {
    /**
     * Called to signal an omnibox session is about to begin or end. This may end a running
     * animation but will not start one.
     */
    void onOmniboxSessionStateChange(boolean active);

    /**
     * Returns the OmniboxAnimator managed by this driver. To start the animation, start() must be
     * called on it explicitly.
     */
    OmniboxAnimator getAnimator();
}
