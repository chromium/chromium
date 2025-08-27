// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

/** The set of operations to inform the view delegate about UI events. */
@NullMarked
public interface PlusAddressCreationDelegate {
    /** Called when the sninner before the generated plus address gets hidden. */
    void onPlusAddressLoadingViewHidden();

    /** Called when the user clicks the refresh button next to the generated plus address. */
    void onRefreshClicked();

    /** Called when the user clicks the confirm button. */
    void onConfirmRequested();

    /** Called when the confirmation loading view is hidden. */
    void onConfirmationLoadingViewHidden();

    /** Called when the user clicks the "Try again" button on the error screen. */
    void onTryAgain();

    /**
     * Called when the user wants to close the bottom sheet by clicking the "Cancel" button on the
     * bottom sheet.
     */
    void onCanceled();

    /** Called by the backend when the generated plus address was confirmed. */
    void onConfirmFinished();

    /** Called when the user closes the bottom sheet by swiping it down, etc. */
    void onPromptDismissed();

    /** Called when the user clicks on a link in the bottom sheet description. */
    void openUrl(GURL url);
}
