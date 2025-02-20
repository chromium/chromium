// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;

/** The interface for displaying dialogs. */
@NullMarked
public interface DialogController {
    /**
     * Show a non-blocking, informational dialog about the contents of an IS_READY_TO_PAY intent.
     *
     * @param readyToPayDebugInfo The informational message to display in a dialog for debugging
     *     purposes. Should not be null.
     */
    void showReadyToPayDebugInfo(String readyToPayDebugInfo);

    /**
     * Show a blocking warning about leaving incognito mode with a prompt to continue into the
     * payment app.
     *
     * @param denyCallback The callback invoked when the user denies or dismisses the prompt. The
     *     callback takes a string parameter to be returned to the merchant website's web developer
     *     in a JavaScript error message. Should not be null.
     * @param approveCallback The callback invoked when the user approves the prompt. The callback
     *     can be invoked immediately in case of an error (e.g., there's no UI) or in embedders that
     *     do not have the concept of incognito mode (e.g., in WebView). Should not be null.
     */
    void showLeavingIncognitoWarning(Callback<String> denyCallback, Runnable approveCallback);
}
