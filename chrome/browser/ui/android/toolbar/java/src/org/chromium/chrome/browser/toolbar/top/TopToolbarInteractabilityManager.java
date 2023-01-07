// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import androidx.annotation.NonNull;

import org.chromium.ui.util.TokenHolder;

/**
 * A class that allow non toolbar clients to communicate with coordinators of top toolbar
 * to update the interactability of top toolbar elements.
 */
public class TopToolbarInteractabilityManager {
    /**
     * An interface for top toolbar coordinators via which it can listen for signals from clients
     * to update the interactability of a toolbar element.
     */
    interface Delegate {
        /**
         * This is fired when a client has requested to modify the interactability of the
         * new tab button view.
         *
         * @param enabled True, if the new tab button needs to be enabled, false otherwise.
         */
        void setNewTabButtonEnabled(boolean enabled);
    }

    /**
     * The {@link Delegate} that relays the events concerning the interactability of top toolbar
     * elements to top toolbar coordinator.
     */
    private final @NonNull Delegate mDelegate;

    /** Token for controlling the interactability of the new tab button. */
    private final TokenHolder mNewTabInteractabilityTokenHolder =
            new TokenHolder(() -> onNewTabButtonTokenUpdate());

    /**
     * @param delegate The {@link Delegate} that is responsible for relaying signal to toolbar
     *         coordinator.
     */
    TopToolbarInteractabilityManager(@NonNull Delegate delegate) {
        mDelegate = delegate;
    }

    /**
     * A method to disable the interactability state of the new tab button in top toolbar element.
     *
     * @return A token which the client can use to re-enable the new tab button.
     */
    public int disableNewTabButton() {
        return mNewTabInteractabilityTokenHolder.acquireToken();
    }

    /**
     * A method to enable the new tab button.
     *
     * Note that the new tab would be enabled if there are no other clients who wants to keep it
     * disable.
     *
     * @param clientToken The token that was returned when disabling the new tab button.
     */
    public void enableNewTabButton(int clientToken) {
        mNewTabInteractabilityTokenHolder.releaseToken(clientToken);
    }

    /**
     * The callback to the new tab token holder which is called when the holder becomes empty or
     * non-empty.
     */
    private void onNewTabButtonTokenUpdate() {
        if (mNewTabInteractabilityTokenHolder.hasTokens()) {
            mDelegate.setNewTabButtonEnabled(/*enabled=*/false);
        } else {
            mDelegate.setNewTabButtonEnabled(/*enabled=*/true);
        }
    }
}
