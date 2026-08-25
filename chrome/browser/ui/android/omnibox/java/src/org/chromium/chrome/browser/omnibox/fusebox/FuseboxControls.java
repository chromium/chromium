// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.omnibox.AutocompleteInput;

/** Interface controlling Fusebox input sessions (e.g. begin, suspend, end). */
@NullMarked
public interface FuseboxControls {
    /**
     * Start a new fusebox input session with the given parameters.
     *
     * @param input The AutocompleteInput to start the session with.
     */
    void beginFuseboxInput(AutocompleteInput input);

    /** End the current fusebox input session. */
    void endFuseboxInput();

    /** Suspend the current fusebox input session. */
    void suspendFuseboxInput();
}
