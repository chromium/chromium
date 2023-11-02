// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf;

import android.content.Context;

import androidx.mediarouter.media.MediaRouteSelector;
import androidx.mediarouter.media.MediaRouter;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.util.ReflectionHelpers;

import java.util.ArrayList;
import java.util.List;

/** Shadow implementation for {@link MediaRouter}. */
@Implements(MediaRouter.class)
public class ShadowMediaRouter {
    private static ShadowImplementation sImpl;

    @Implementation
    public static MediaRouter getInstance(Context context) {
        return ReflectionHelpers.callConstructor(MediaRouter.class);
    }

    @Implementation
    public List<MediaRouter.RouteInfo> getRoutes() {
        return sImpl.getRoutes();
    }

    @Implementation
    public void selectRoute(MediaRouter.RouteInfo route) {
        sImpl.selectRoute(route);
    }

    @Implementation
    public MediaRouter.RouteInfo getDefaultRoute() {
        return sImpl.getDefaultRoute();
    }

    @Implementation
    public void addCallback(MediaRouteSelector selector, MediaRouter.Callback callback, int flags) {
        sImpl.addCallback(selector, callback, flags);
    }

    @Implementation
    public void addCallback(MediaRouteSelector selector, MediaRouter.Callback callback) {
        sImpl.addCallback(selector, callback);
    }

    @Implementation
    public void removeCallback(MediaRouter.Callback callback) {
        sImpl.removeCallback(callback);
    }

    @Implementation
    public void unselect(int reason) {
        sImpl.unselect(reason);
    }

    public static void setImplementation(ShadowImplementation impl) {
        sImpl = impl;
    }

    /** The implementation skeleton for the implementation backing the shadow. */
    public static class ShadowImplementation {
        public List<MediaRouter.RouteInfo> getRoutes() {
            return new ArrayList<>();
        }

        public void selectRoute(MediaRouter.RouteInfo route) {}

        public MediaRouter.RouteInfo getDefaultRoute() {
            return null;
        }

        public void addCallback(
                MediaRouteSelector selector, MediaRouter.Callback callback, int flags) {}

        public void addCallback(MediaRouteSelector selector, MediaRouter.Callback callback) {}

        public void removeCallback(MediaRouter.Callback callback) {}

        public void unselect(int reason) {}
    }
}
