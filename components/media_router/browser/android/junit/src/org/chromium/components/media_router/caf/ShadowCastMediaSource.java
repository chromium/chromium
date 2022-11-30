// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

/** Shadow implementation for {@link MediaRouter}. */
@Implements(CastMediaSource.class)
public class ShadowCastMediaSource {
    private static ShadowImplementation sImpl;

    @Implementation
    public static CastMediaSource from(String sourceId) {
        return sImpl.from(sourceId);
    }

    public static void setImplementation(ShadowImplementation impl) {
        sImpl = impl;
    }

    /** The implementation skeleton for the implementation backing the shadow. */
    public static class ShadowImplementation {
        public CastMediaSource from(String sourceId) {
            return null;
        }
    }
}
