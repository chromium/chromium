// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_relationship_verification;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.components.embedder_support.util.Origin;

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
    private static final String HTTP_SCHEME = "http";
    private static final String HTTPS_SCHEME = "https";

    private OriginVerifier mOriginVerifier;

    /** Origins that we have yet to call OriginVerifier#start or whose validatin is not yet finished. */
    @Nullable private Set<Origin> mPendingOrigins = Collections.synchronizedSet(new HashSet<>());

    public OriginVerificationScheduler(OriginVerifier originVerifier, Set<Origin> pendingOrigins) {
        mOriginVerifier = originVerifier;
        mPendingOrigins = pendingOrigins;
    }

    public Set<Origin> getPendingOriginsForTesting() {
        return mPendingOrigins;
    }

    // Use this function only for testing.
    public void addPendingOriginForTesting(Origin origin) {
        mPendingOrigins.add(origin);
    }

    public void verify(String url, Callback<Boolean> callback) {
        verify(Origin.create(url), callback);
    }

    public void verify(Origin origin, Callback<Boolean> callback) {
        ThreadUtils.assertOnUiThread();
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
            mOriginVerifier.start(
                    (packageName, unused, verified, online) -> {
                        mPendingOrigins.remove(origin);

                        callback.onResult(verified);
                    },
                    origin);
            return;
        }
        callback.onResult(mOriginVerifier.wasPreviouslyVerified(origin));
    }

    public void scheduleAllPendingVerifications(@Nullable Callback<Boolean> callback) {
        ThreadUtils.assertOnUiThread();
        if (callback == null) {
            callback = (res) -> {};
        }
        for (Origin origin : mPendingOrigins) {
            verify(origin, callback);
        }
    }

    public OriginVerifier getOriginVerifier() {
        return mOriginVerifier;
    }
}
