// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.build.annotations.NullMarked;

@NullMarked
public interface SuggestionsListAnimationDriver {
    /**
     * Called to signal an omnibox session is about to begin or end so that the driver can begin
     * animating the associated transition, if any.
     */
    void onOmniboxSessionStateChange(boolean active);

    /**
     * Whether animation is currently enabled. If false, the driver does not expect to control
     * animation for the current session.
     */
    boolean isAnimationEnabled();
}
