// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill.ephemeraltab;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;

/** The coordinator for triggering the Ephemeral Tab. */
@NullMarked
class PaymentsWindowCoordinator {
    private final WebContents mWebContents;

    /** Constructs a new {@code PaymentsWindowCoordinator} from the provided {@code WebContents}. */
    PaymentsWindowCoordinator(WebContents webContents) {
        mWebContents = webContents;
    }

    /**
     * Attempts to open an ephemeral tab; it involves obtaining the {@code WindowAndroid} from the
     * managed {@code WebContents} and using it to present the UI.
     */
    void openEphemeralTab() {
        // TODO(crbug.com/430575808): Implement the connection to trigger the Ephemeral Tab.
    }

    WebContents getWebContentsForTesting() {
        return mWebContents;
    }
}
