// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf;

import android.content.Context;

import com.google.android.gms.cast.framework.CastContext;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

/** Shadow for {@link CastContext}. */
@Implements(CastContext.class)
public class ShadowCastContext {
    private static CastContext sInstance;

    @Implementation
    public static CastContext getSharedInstance(Context context) {
        return sInstance;
    }

    public static void setInstance(CastContext instance) {
        sInstance = instance;
    }
}
