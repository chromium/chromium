// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import org.chromium.build.annotations.NullMarked;

/**
 * Interface representing the host (display channel) for the enterprise signals disclaimer. It
 * abstracts the display logic which could be a Bottom Sheet or a Modal Dialog.
 */
@NullMarked
interface EnterpriseSignalsDisclaimerHost {
    /**
     * Attempts to show the enterprise signals disclaimer. If the dialog cannot be shown it will be
     * put in a queue and shown whenever possible.
     */
    void show();

    /**
     * @return true if dialog is being shown or is in queue, false otherwise.
     */
    boolean isActive();

    /** Hides and dismisses the disclaimer UI. */
    void hide();
}
