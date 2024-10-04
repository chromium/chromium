// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge;

import android.view.Window;

import androidx.core.view.WindowCompat;

import org.chromium.base.UnownedUserData;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.ui.util.TokenHolder;

/**
 * Activity level coordinator that manages edge to edge related interactions.
 *
 * <p>{@link #get()} never returns null for this class.
 */
public class EdgeToEdgeStateProvider extends ObservableSupplierImpl<Boolean>
        implements UnownedUserData {
    private final TokenHolder mTokenHolder = new TokenHolder(this::onTokenUpdate);
    private final Window mWindow;

    public EdgeToEdgeStateProvider(Window window) {
        super(/* initialValue= */ false);
        mWindow = window;
    }

    /**
     * Request a call to draw edge to edge, equivalent to {@code
     * Window.setDecorFitsSystemWindows(false)}.
     *
     * @return A token to release the edge to edge state
     */
    public int acquireSetDecorFitsSystemWindowToken() {
        return mTokenHolder.acquireToken();
    }

    /**
     * Release a token to edge to edge. When the token holder is empty, trigger a call to {@code
     * Window.setDecorFitsSystemWindows(true)}.
     */
    public void releaseSetDecorFitsSystemWindowToken(int token) {
        mTokenHolder.releaseToken(token);
    }

    private void onTokenUpdate() {
        boolean isEdgeToEdge = mTokenHolder.hasTokens();
        if (isEdgeToEdge == get()) {
            return;
        }

        // Edge to edge mode changed.
        WindowCompat.setDecorFitsSystemWindows(mWindow, !isEdgeToEdge);
        set(isEdgeToEdge);
    }
}
