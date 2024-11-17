// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import org.chromium.url.GURL;

/** The set of operations to inform the view delegate about UI events. */
public interface PlusAddressCreationDelegate {
    /** Called when the sninner before the generated plus address gets hidden. */
    public void onPlusAddressLoadingViewHidden();

    /** Called when the user clicks the refresh button next to the generated plus address. */
    public void onRefreshClicked();

    /** Called when the user clicks the confirm button. */
    public void onConfirmRequested();

    /** Called when the confirmation loading view is hidden. */
    public void onConfirmationLoadingViewHidden();

    /** Called when the user clicks the "Try again" button on the error screen. */
    public void onTryAgain();

    /**
     * Called when the user wants to close the bottom sheet by clicking the "Cancel" button on the
     * bottom sheet.
     */
    public void onCanceled();

    /** Called by the backend when the generated plus address was confirmed. */
    public void onConfirmFinished();

    /** Called when the user closes the bottom sheet by swiping it down, etc. */
    public void onPromptDismissed();

    /** Called when the user clicks on a link in the bottom sheet description. */
    public void openUrl(GURL url);
}
