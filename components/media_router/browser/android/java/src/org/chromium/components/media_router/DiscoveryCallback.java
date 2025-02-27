// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import androidx.mediarouter.media.MediaRouteSelector;
import androidx.mediarouter.media.MediaRouter;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Callback for discovering routes with one particular filter. Keeps a set of all source URIs that
 * media sinks were requested for. Once a route is added or removed, updates the
 * {@link BrowserMediaRouter} with the new routes.
 */
public class DiscoveryCallback extends MediaRouter.Callback {
    private final DiscoveryDelegate mDiscoveryDelegate;
    private final MediaRouteSelector mRouteSelector;
    private Set<String> mSourceUrns = new HashSet<String>();
    private List<MediaSink> mSinks = new ArrayList<MediaSink>();

    public DiscoveryCallback(
            String sourceUrn, DiscoveryDelegate delegate, MediaRouteSelector selector) {
        assert delegate != null;
        assert sourceUrn != null && !sourceUrn.isEmpty();

        mSourceUrns.add(sourceUrn);
        mDiscoveryDelegate = delegate;
        mRouteSelector = selector;
    }

    public DiscoveryCallback(
            String sourceUrn,
            List<MediaSink> knownSinks,
            DiscoveryDelegate delegate,
            MediaRouteSelector selector) {
        this(sourceUrn, delegate, selector);
        setAndUpdateSinks(knownSinks);
    }

    public void addSourceUrn(String sourceUrn) {
        if (mSourceUrns.add(sourceUrn)) {
            mDiscoveryDelegate.onSinksReceived(sourceUrn, new ArrayList<MediaSink>(mSinks));
        }
    }

    public boolean containsSourceUrn(String sourceUrn) {
        return mSourceUrns.contains(sourceUrn);
    }

    public void removeSourceUrn(String sourceUrn) {
        mSourceUrns.remove(sourceUrn);
    }

    public boolean isEmpty() {
        return mSourceUrns.isEmpty();
    }

    public void setAndUpdateSinks(List<MediaSink> knownSinks) {
        mSinks = knownSinks;
        updateBrowserMediaRouter();
    }

    @Override
    public void onRouteAdded(MediaRouter router, MediaRouter.RouteInfo route) {
        if (route == null || !route.matchesSelector(mRouteSelector)) return;

        MediaSink sink = MediaSink.fromRoute(route);
        if (mSinks.contains(sink)) return;
        mSinks.add(sink);
        updateBrowserMediaRouter();
    }

    @Override
    public void onRouteRemoved(MediaRouter router, MediaRouter.RouteInfo route) {
        MediaSink sink = MediaSink.fromRoute(route);
        if (!mSinks.contains(sink)) return;
        mSinks.remove(sink);
        updateBrowserMediaRouter();
    }

    @Override
    public void onRouteChanged(MediaRouter router, MediaRouter.RouteInfo route) {
        // Sometimes onRouteAdded is not called for the route as it doesn't yet match the selector.
        // onRouteChanged() will be called later when the matching category is added.
        if (route == null) return;

        if (route.matchesSelector(mRouteSelector)) {
            onRouteAdded(router, route);
        } else {
            onRouteRemoved(router, route);
        }
    }

    private void updateBrowserMediaRouter() {
        for (String sourceUrn : mSourceUrns) {
            mDiscoveryDelegate.onSinksReceived(sourceUrn, new ArrayList<MediaSink>(mSinks));
        }
    }
}
