// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf;

import com.google.android.gms.cast.framework.CastContext;

import org.chromium.components.media_router.MediaRouterClient;

/** Utility methods for Cast. */
public class CastUtils {
    /** Helper method to return the {@link CastContext} instance. */
    public static CastContext getCastContext() {
        return CastContext.getSharedInstance(
                MediaRouterClient.getInstance().getContextForRemoting());
    }

    /**
     * Compares two origins. Empty origin strings correspond to unique origins in
     * url::Origin.
     *
     * @param originA A URL origin.
     * @param originB A URL origin.
     * @return True if originA and originB represent the same origin, false otherwise.
     */
    public static final boolean isSameOrigin(String originA, String originB) {
        if (originA == null || originA.isEmpty() || originB == null || originB.isEmpty()) {
            return false;
        }
        return originA.equals(originB);
    }
}
