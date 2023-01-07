// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.cast_emulator;

import android.content.Context;

import androidx.mediarouter.media.MediaRouteDiscoveryRequest;
import androidx.mediarouter.media.MediaRouteProvider;
import androidx.mediarouter.media.MediaRouteSelector;

import org.chromium.base.Log;
import org.chromium.components.media_router.cast_emulator.remote.RemotePlaybackRoutePublisher;
import org.chromium.components.media_router.cast_emulator.router.DummyRoutePublisher;

import java.util.ArrayList;
import java.util.List;

/**
 * A test MRP that registers some dummy media sinks to the Android support library, so that these
 * dummy sinks can be discovered and shown in the device selection dialog in media router tests and
 * media remote tests. The class publish different routes (sinks) according to {@link
 * MediaRouteDiscoveryRequest}.
 */
final class TestMediaRouteProvider extends MediaRouteProvider {
    private static final String TAG = "TestMRP";

    private List<RoutePublisher> mRoutePublishers = new ArrayList<RoutePublisher>();

    public TestMediaRouteProvider(Context context) {
        super(context);

        mRoutePublishers.add(new RemotePlaybackRoutePublisher(this));
        mRoutePublishers.add(new DummyRoutePublisher(this));
    }

    @Override
    public void onDiscoveryRequestChanged(MediaRouteDiscoveryRequest request) {
        Log.i(TAG, "discoveryRequestChanged : " + request);
        if (request != null) {
            MediaRouteSelector selector = request.getSelector();
            if (selector != null) {
                List<String> controlCategories = selector.getControlCategories();
                for (RoutePublisher publisher : mRoutePublishers) {
                    for (String controlCategory : controlCategories) {
                        if (publisher.supportsControlCategory(controlCategory)) {
                            publisher.publishRoutes();
                            return;
                        }
                    }
                }
            }
        }
        Log.i(TAG, "no route publisher supports the request, not publishing routes");
    }

    @Override
    public RouteController onCreateRouteController(String routeId) {
        for (RoutePublisher publisher : mRoutePublishers) {
            if (publisher.supportsRoute(routeId)) return publisher.onCreateRouteController(routeId);
        }
        return null;
    }
}
