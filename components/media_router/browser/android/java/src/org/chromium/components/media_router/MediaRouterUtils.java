// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import android.app.Application;
import android.content.Context;
import android.content.ContextWrapper;

import org.chromium.base.ContextUtils;

/** Utility methods for Media Router. */
public class MediaRouterUtils {
    /**
     * Helper method to return the {@link Context} instance to be used for {@link MediaRouter} and
     * {@link CastContext}.
     */
    public static Context getContextForCasting() {
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
        return context;
    }
}
