// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/** Mocked {@link MediaRouteProvider}. */
public class MockMediaRouteProvider implements MediaRouteProvider {
    private static final String TAG = "MediaRouter";

    private static final String SINK_ID1 = "test_sink_id_1";
    private static final String SINK_ID2 = "test_sink_id_2";
    private static final String SINK_NAME1 = "test-sink-1";
    private static final String SINK_NAME2 = "test-sink-2";

    private MediaRouteManager mManager;

    private final Map<String, MediaRoute> mRoutes = new HashMap<String, MediaRoute>();
    private final Map<String, MediaRoute> mPresentationIdToRoute =
            new HashMap<String, MediaRoute>();

    private int mSinksObservedDelayMillis;
    private int mCreateRouteDelayMillis;
    private boolean mIsSupportsSource = true;
    private String mCreateRouteErrorMessage;
    private String mJoinRouteErrorMessage;
    private boolean mCloseRouteWithErrorOnSend;

    /** Factory for {@link MockMediaRouteProvider}. */
    public static class Factory implements MediaRouteProvider.Factory {
        public static final MockMediaRouteProvider sProvider = new MockMediaRouteProvider();

        @Override
        public void addProviders(MediaRouteManager manager) {
            sProvider.mManager = manager;
            manager.addMediaRouteProvider(sProvider);
        }
    }

    private MockMediaRouteProvider() {}

    public void setCreateRouteDelayMillis(int delayMillis) {
        assert delayMillis >= 0;
        mCreateRouteDelayMillis = delayMillis;
    }

    public void setSinksObservedDelayMillis(int delayMillis) {
        assert delayMillis >= 0;
        mSinksObservedDelayMillis = delayMillis;
    }

    public void setIsSupportsSource(boolean isSupportsSource) {
        mIsSupportsSource = isSupportsSource;
    }

    public void setCreateRouteErrorMessage(String errorMessage) {
        mCreateRouteErrorMessage = errorMessage;
    }

    public void setJoinRouteErrorMessage(String errorMessage) {
        mJoinRouteErrorMessage = errorMessage;
    }

    public void setCloseRouteWithErrorOnSend(boolean closeRouteWithErrorOnSend) {
        mCloseRouteWithErrorOnSend = closeRouteWithErrorOnSend;
    }

    @Override
    public boolean supportsSource(String sourceId) {
        return mIsSupportsSource;
    }

    @Override
    public void startObservingMediaSinks(final String sourceId) {
        final ArrayList<MediaSink> sinks = new ArrayList<MediaSink>();
        sinks.add(new MediaSink(SINK_ID1, SINK_NAME1, null));
        sinks.add(new MediaSink(SINK_ID2, SINK_NAME2, null));
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT,
                () -> mManager.onSinksReceived(sourceId, MockMediaRouteProvider.this, sinks),
                mSinksObservedDelayMillis);
    }

    @Override
    public void stopObservingMediaSinks(String sourceId) {}

    @Override
    public void createRoute(
            final String sourceId,
            final String sinkId,
            final String presentationId,
            final String origin,
            final int tabId,
            final boolean isIncognito,
            final int nativeRequestId) {
        if (mCreateRouteErrorMessage != null) {
            mManager.onCreateRouteRequestError(mCreateRouteErrorMessage, nativeRequestId);
            return;
        }

        if (mCreateRouteDelayMillis == 0) {
            doCreateRoute(sourceId, sinkId, presentationId, origin, tabId, nativeRequestId);
        } else {
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    () ->
                            doCreateRoute(
                                    sourceId,
                                    sinkId,
                                    presentationId,
                                    origin,
                                    tabId,
                                    nativeRequestId),
                    mCreateRouteDelayMillis);
        }
    }

    private void doCreateRoute(
            String sourceId,
            String sinkId,
            String presentationId,
            String origin,
            int tabId,
            int nativeRequestId) {
        MediaRoute route = new MediaRoute(sinkId, sourceId, presentationId);
        mRoutes.put(route.id, route);
        mPresentationIdToRoute.put(presentationId, route);
        mManager.onRouteCreated(route.id, sinkId, nativeRequestId, this, true);
    }

    @Override
    public void joinRoute(
            String sourceId, String presentationId, String origin, int tabId, int nativeRequestId) {
        if (mJoinRouteErrorMessage != null) {
            mManager.onJoinRouteRequestError(mJoinRouteErrorMessage, nativeRequestId);
            return;
        }
        MediaRoute existingRoute = mPresentationIdToRoute.get(presentationId);
        if (existingRoute == null) {
            mManager.onJoinRouteRequestError("Presentation does not exist", nativeRequestId);
            return;
        }
        mManager.onRouteCreated(
                existingRoute.id, existingRoute.sinkId, nativeRequestId, this, true);
    }

    @Override
    public void closeRoute(String routeId) {
        MediaRoute route = mRoutes.get(routeId);
        if (route == null) {
            Log.i(TAG, "closeRoute: Route does not exist: " + routeId);
            return;
        }
        mRoutes.remove(routeId);
        Map<String, MediaRoute> newPresentationIdToRoute = new HashMap<String, MediaRoute>();
        for (Map.Entry<String, MediaRoute> entry : mPresentationIdToRoute.entrySet()) {
            if (!entry.getValue().id.equals(routeId)) {
                newPresentationIdToRoute.put(entry.getKey(), entry.getValue());
            }
        }
        mPresentationIdToRoute.clear();
        mPresentationIdToRoute.putAll(newPresentationIdToRoute);
        mManager.onRouteTerminated(routeId);
    }

    @Override
    public void detachRoute(String routeId) {}

    @Override
    public void sendStringMessage(String routeId, String message) {
        if (mCloseRouteWithErrorOnSend) {
            mManager.onRouteClosed(routeId, "Sending message failed. Closing the route.");
        } else {
            mManager.onMessage(routeId, "Pong: " + message);
        }
    }

    @Override
    @Nullable
    public FlingingController getFlingingController(String routeId) {
        return null;
    }
}
