// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.content.Context;
import androidx.annotation.Nullable;

/**
 * Used to initialize singleton-level dependencies for xsurface. Also provides surface-level
 * dependencies that depend on the singleton dependencies.
 **/
public interface ProcessScope {
    /**
     * To be called after a login state change event, will cause the next SurfaceScope to use fresh
     * account-level dependencies.
     */
    default void resetAccount() {}

    /**
     * Returns a SurfaceScope which should be one per Surface. That Surface can have multiple
     * HybridListRenderers and SurfaceRenderers within its UI.
     *
     * @param dependencyProvider Provider for activity-scoped dependencies.
     **/
    @Nullable
    default SurfaceScope obtainSurfaceScope(SurfaceScopeDependencyProvider dependencyProvider) {
        return obtainSurfaceScope(dependencyProvider.getActivityContext());
    }

    @Nullable
    @Deprecated
    default SurfaceScope obtainSurfaceScope(Context activityContext) {
        return null;
    }

    @Nullable
    default ImagePrefetcher provideImagePrefetcher() {
        return null;
    }
}
