// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox.consent;

import android.webkit.JavascriptInterface;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.UsedByReflection;

import java.lang.ref.WeakReference;

/**
 * JavaScript interface bridge injected into the ConsentKit web page as {@code window.ckUi}.
 *
 * <p>Receives lifecycle and result events from the ConsentKit web page and dispatches them to the
 * {@link Delegate} on the UI thread.
 */
@NullMarked
class DriveConsentJsBridge {
    /** Interface for receiving events dispatched from the ConsentKit web page. */
    interface Delegate {
        /**
         * Called when the ConsentKit page finishes its initialization handshake.
         *
         * @param callbackId The identifier passed by the page to be returned via {@code
         *     window.ckUiCallback(callbackId)}.
         */
        void onWhenAttached(int callbackId);

        /**
         * Called when the ConsentKit page finishes its consent flow.
         *
         * @param resultJson The JSON payload containing the flow result, or null if the page closed
         *     without providing a result.
         */
        void onCloseWithResult(@Nullable String resultJson);
    }

    private final WeakReference<Delegate> mDelegateRef;

    /**
     * Constructs a new JavaScript bridge.
     *
     * @param delegate The delegate to receive callbacks on the UI thread.
     */
    DriveConsentJsBridge(Delegate delegate) {
        mDelegateRef = new WeakReference<>(delegate);
    }

    /** Called by the ConsentKit page when it attaches to the native container. */
    @JavascriptInterface
    @UsedByReflection("ConsentKit WebView JS bridge (window.ckUi)")
    public void whenAttached(int callbackId) {
        ThreadUtils.postOnUiThread(
                () -> {
                    Delegate delegate = mDelegateRef.get();
                    if (delegate != null) {
                        delegate.onWhenAttached(callbackId);
                    }
                });
    }

    /** No-op since {@link #closeWithResult} supplies the complete flow result. */
    @JavascriptInterface
    @UsedByReflection("ConsentKit WebView JS bridge (window.ckUi)")
    public void setResult(@Nullable String resultJson) {}

    /**
     * Called by the ConsentKit page to close the dialog with the given result payload.
     *
     * @param resultJson The final JSON payload containing the flow result.
     */
    @JavascriptInterface
    @UsedByReflection("ConsentKit WebView JS bridge (window.ckUi)")
    public void closeWithResult(@Nullable String resultJson) {
        ThreadUtils.postOnUiThread(
                () -> {
                    Delegate delegate = mDelegateRef.get();
                    if (delegate != null) {
                        delegate.onCloseWithResult(resultJson);
                    }
                });
    }
}
