// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Promise;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.webid.DigitalIdentityUiWarningLevel;
import org.chromium.ui.base.WindowAndroid;

@NullMarked
public class IdentityCredentialsDelegate {
    private static final String TAG = "IdentityCredentials";

    public static class DigitalCredential {
        public String mProtocol;
        public String mData;

        public DigitalCredential(String protocol, String data) {
            this.mProtocol = protocol;
            this.mData = data;
        }
    }

    // Tracks the currently active presentation or creation delegate so `cancel()` can
    // tear down its in-flight PendingIntent when the owning provider is destroyed.
    private @Nullable DigitalCredentialsDelegateBase mActiveDelegate;

    @VisibleForTesting
    DigitalCredentialsPresentationDelegate createPresentationDelegate() {
        return new DigitalCredentialsPresentationDelegate();
    }

    @VisibleForTesting
    DigitalCredentialsCreationDelegate createCreationDelegate() {
        return new DigitalCredentialsCreationDelegate();
    }

    public Promise<DigitalCredential> get(
            WindowAndroid window,
            String origin,
            String request,
            @DigitalIdentityUiWarningLevel int warningLevel) {
        assert mActiveDelegate == null;
        DigitalCredentialsPresentationDelegate delegate = createPresentationDelegate();
        mActiveDelegate = delegate;
        return delegate.get(window, origin, request, warningLevel)
                .andFinally(() -> mActiveDelegate = null);
    }

    public Promise<DigitalCredential> create(WindowAndroid window, String origin, String request) {
        assert mActiveDelegate == null;
        DigitalCredentialsCreationDelegate delegate = createCreationDelegate();
        mActiveDelegate = delegate;
        return delegate.create(window, origin, request).andFinally(() -> mActiveDelegate = null);
    }

    /**
     * Cancels any active in-flight presentation or creation delegate.
     *
     * <p>Concurrent requests within the same frame are rejected by C++ {@code
     * DigitalIdentityRequestImpl}, and each native provider owns a distinct Java {@link
     * DigitalIdentityProvider} instance.
     */
    public void cancel() {
        if (mActiveDelegate != null) {
            mActiveDelegate.cancel();
            mActiveDelegate = null;
        }
    }
}
