// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.digital_asset_links;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * OriginVerificationScheduler manages the scheduling of verification requests based on an {@link
 * OriginVerifier} and a list of pending origins. It is safe to call {@link
 * OriginVerificationScheduler#verify} several times, the request for the statement list on the
 * website will only performed at most once.
 */
public class OriginVerificationScheduler {
    private static final String TAG = "OriginVerification";

    private static final String HTTP_SCHEME = "http";
    private static final String HTTPS_SCHEME = "https";

    private OriginVerifier mOriginVerifier;

    /**
     * Origins that we have yet to call OriginVerifier#start or whose validatin is not yet finished.
     */
    @Nullable
    private Set<Origin> mPendingOrigins = Collections.synchronizedSet(new HashSet<>());

    public OriginVerificationScheduler(OriginVerifier originVerifier, Set<Origin> pendingOrigins) {
        mOriginVerifier = originVerifier;
        mPendingOrigins = pendingOrigins;
    }

    @VisibleForTesting
    public Set<Origin> addPendingOriginForTesting() {
        return mPendingOrigins;
    }

    // Use this function only for testing.
    @VisibleForTesting
    public void addPendingOrigin(Origin origin) {
        mPendingOrigins.add(origin);
    }

    public void verify(
            String url, BrowserContextHandle browserContextHandle, Callback<Boolean> callback) {
        ThreadUtils.assertOnUiThread();

        Origin origin = Origin.create(url);
        if (origin == null) {
            callback.onResult(false);
            return;
        }
        String urlScheme = origin.uri().getScheme();
        if (!urlScheme.equals(HTTPS_SCHEME) && !urlScheme.equals(HTTP_SCHEME)) {
            callback.onResult(true);
            return;
        }

        if (mPendingOrigins.contains(origin)) {
            mOriginVerifier.start((packageName, unused, verified, online) -> {
                mPendingOrigins.remove(origin);

                callback.onResult(verified);
            }, browserContextHandle, origin);
            return;
        }
        callback.onResult(mOriginVerifier.wasPreviouslyVerified(origin));
    }
}
