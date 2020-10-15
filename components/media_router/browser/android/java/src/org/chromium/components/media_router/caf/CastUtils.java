// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf;

import android.app.Application;
import android.content.Context;
import android.content.ContextWrapper;

import com.google.android.gms.cast.framework.CastContext;

import org.chromium.base.ContextUtils;

/** Utility methods for Cast. */
public class CastUtils {
    /** Helper method to return the {@link CastContext} instance. */
    public static CastContext getCastContext() {
        Context context = ContextUtils.getApplicationContext();
        // The GMS Cast framework assumes the passed {@link Context} returns an instance of {@link
        // Application} from {@link getApplicationContext()}, so we make sure to remove any
        // wrappers.
        while (!(context.getApplicationContext() instanceof Application)) {
            if (context instanceof ContextWrapper) {
                context = ((ContextWrapper) context).getBaseContext();
            } else {
                return null;
            }
        }
        return CastContext.getSharedInstance(context);
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
