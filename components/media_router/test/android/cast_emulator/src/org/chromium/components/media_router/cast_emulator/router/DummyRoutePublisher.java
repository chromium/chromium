// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.cast_emulator.router;

import android.content.IntentFilter;

import androidx.mediarouter.media.MediaRouteDescriptor;
import androidx.mediarouter.media.MediaRouteProvider;
import androidx.mediarouter.media.MediaRouteProviderDescriptor;

import com.google.android.gms.cast.CastMediaControlIntent;

import org.chromium.base.Log;
import org.chromium.components.media_router.cast_emulator.RoutePublisher;

import java.util.ArrayList;

/**
 * A dummy route publisher that registers some dummy media sinks to the Android support library, so
 * that these dummy sinks can be discovered and shown in the device selection dialog in media router
 * tests.  The Cast app id must be fixed to "CCCCCCCC" so that these sinks can be detected.
 */
public final class DummyRoutePublisher implements RoutePublisher {
    private static final String TAG = "DummyRoutePublisher";

    private static final String DUMMY_ROUTE_ID1 = "test_sink_id_1";
    private static final String DUMMY_ROUTE_ID2 = "test_sink_id_2";
    private static final String DUMMY_ROUTE_NAME1 = "test-sink-1";
    private static final String DUMMY_ROUTE_NAME2 = "test-sink-2";

    private static final String CAST_APP_ID = "CCCCCCCC";

    private final MediaRouteProvider mProvider;

    public DummyRoutePublisher(MediaRouteProvider provider) {
        mProvider = provider;
    }

    @Override
    public boolean supportsControlCategory(String controlCategory) {
        return controlCategory.equals(CastMediaControlIntent.categoryForCast(CAST_APP_ID));
    }

    @Override
    public void publishRoutes() {
        Log.i(TAG, "Publishing routes");
        IntentFilter filter = new IntentFilter();
        filter.addCategory(CastMediaControlIntent.categoryForCast("CCCCCCCC"));
        filter.addDataScheme("http");
        filter.addDataScheme("https");
        filter.addDataScheme("file");

        ArrayList<IntentFilter> controlFilters = new ArrayList<IntentFilter>();
        controlFilters.add(filter);

        MediaRouteDescriptor testRouteDescriptor1 =
                new MediaRouteDescriptor.Builder(DUMMY_ROUTE_ID1, DUMMY_ROUTE_NAME1)
                        .setDescription(DUMMY_ROUTE_NAME1)
                        .addControlFilters(controlFilters)
                        .build();
        MediaRouteDescriptor testRouteDescriptor2 =
                new MediaRouteDescriptor.Builder(DUMMY_ROUTE_ID2, DUMMY_ROUTE_NAME2)
                        .setDescription(DUMMY_ROUTE_NAME2)
                        .addControlFilters(controlFilters)
                        .build();

        MediaRouteProviderDescriptor providerDescriptor =
                new MediaRouteProviderDescriptor.Builder()
                        .addRoute(testRouteDescriptor1)
                        .addRoute(testRouteDescriptor2)
                        .build();
        mProvider.setDescriptor(providerDescriptor);
    }

    @Override
    public boolean supportsRoute(String routeId) {
        return false;
    }

    @Override
    public MediaRouteProvider.RouteController onCreateRouteController(String routeId) {
        return null;
    }
}
