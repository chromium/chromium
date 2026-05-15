// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf.remoting;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.mediarouter.media.MediaRouteSelector;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.components.media_router.DiscoveryCallback;
import org.chromium.components.media_router.MediaRoute;
import org.chromium.components.media_router.MediaRouteManager;
import org.chromium.components.media_router.MediaRouterClient;
import org.chromium.components.media_router.MediaSink;
import org.chromium.components.media_router.MediaSource;
import org.chromium.components.media_router.TestMediaRouterClient;
import org.chromium.components.media_router.caf.CreateRouteRequestInfo;
import org.chromium.components.media_router.caf.MediaRouterTestHelper;
import org.chromium.components.media_router.caf.ShadowMediaRouter;

/** Robolectric tests for CafRemotingMediaRouteProvider. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowMediaRouter.class},
        // Required to mock final.
        instrumentedPackages = {"androidx.mediarouter.media.MediaRouteSelector"})
public class CafRemotingMediaRouteProviderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private CafRemotingMediaRouteProvider mProvider;
    private MediaRouterTestHelper mMediaRouterHelper;
    @Mock private MediaRouteManager mManager;
    @Mock private RemotingSessionController mSessionController;

    @Before
    public void setUp() {
        mMediaRouterHelper = new MediaRouterTestHelper();
        MediaRouterClient.setInstance(new TestMediaRouterClient());
        mProvider = spy(CafRemotingMediaRouteProvider.create(mManager));
        doReturn(mSessionController).when(mProvider).sessionController();
    }

    @After
    public void tearDown() {
        MediaRouterClient.setInstance(null);
    }

    @Test
    public void testStartObservingMediaSinks_updateMediaSourceIfNeeded() {
        String appId = "app-id";
        String sourceId1 = "source-id-1";
        String sourceId2 = "source-id-2";
        String origin = "origin";
        MediaSource mockSource1 = mock(MediaSource.class);
        MediaSource mockSource2 = mock(MediaSource.class);
        MediaRouteSelector mockSelector1 = mock(MediaRouteSelector.class);
        MediaRouteSelector mockSelector2 = mock(MediaRouteSelector.class);
        prepareMediaSource(mockSource1, mockSelector1, sourceId1, appId);
        prepareMediaSource(mockSource2, mockSelector2, sourceId2, appId);

        // No active session, so no media source update.
        mProvider.startObservingMediaSinks(sourceId2, origin);
        verify(mProvider).updateSessionMediaSourceIfNeeded(eq(null), eq(mockSource2), eq(origin));
        verify(mSessionController, never()).updateMediaSource(any(MediaSource.class));
        mProvider.stopObservingMediaSinks(sourceId2);

        String presentationId = "presentation-id";
        String sinkId = "sink-id";
        MediaSink sink = mock(MediaSink.class);
        doReturn(sinkId).when(sink).getId();

        mProvider.startObservingMediaSinks(sourceId1, origin);

        CreateRouteRequestInfo info =
                new CreateRouteRequestInfo(
                        mockSource1,
                        sink,
                        presentationId,
                        origin,
                        1,
                        false,
                        1,
                        mMediaRouterHelper.getCastRoute());
        MediaRoute route = new MediaRoute(sinkId, sourceId1, presentationId);
        assertEquals(route.id, info.routeId);
        mProvider.addRouteForTesting(route, origin, 1, 1, false);
        FlingingControllerAdapter flingingControllerAdapter = mock(FlingingControllerAdapter.class);
        doReturn(true).when(mSessionController).isConnected();
        doReturn(flingingControllerAdapter).when(mSessionController).getFlingingController();
        doReturn(info).when(mSessionController).getRouteCreationInfo();

        // `sourceId1` is still being observed, so no media source update.
        mProvider.startObservingMediaSinks(sourceId2, origin);
        verify(mProvider)
                .updateSessionMediaSourceIfNeeded(
                        any(DiscoveryCallback.class), eq(mockSource2), eq(origin));
        verify(mSessionController, never()).updateMediaSource(any(MediaSource.class));
        mProvider.stopObservingMediaSinks(sourceId2);

        mProvider.stopObservingMediaSinks(sourceId1);
        doReturn(mockSource1).when(mProvider).getSourceFromId(sourceId1);
        assertEquals(mProvider.getActiveRoutesForTesting().get(route.id).getSourceId(), sourceId1);

        // Origin mismatch, should not update.
        doReturn(mockSource2).when(mProvider).getSourceFromId(sourceId2);
        mProvider.startObservingMediaSinks(sourceId2, "attacker-origin");
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mProvider)
                .updateSessionMediaSourceIfNeeded(eq(null), eq(mockSource2), eq("attacker-origin"));
        verify(mSessionController, never()).updateMediaSource(any(MediaSource.class));
        mProvider.stopObservingMediaSinks(sourceId2);

        // Route media source should be updated when origin matches.
        mProvider.startObservingMediaSinks(sourceId2, origin);
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mProvider, times(2))
                .updateSessionMediaSourceIfNeeded(eq(null), eq(mockSource2), eq(origin));
        verify(mSessionController).updateMediaSource(eq(mockSource2));

        mProvider.updateRouteMediaSource(route.id, sourceId2);
        assertEquals(1, mProvider.getActiveRoutesForTesting().size());
        assertEquals(mProvider.getActiveRoutesForTesting().get(route.id).getSourceId(), sourceId2);
        verify(mManager).onRouteMediaSourceUpdated(route.id, sourceId2);
    }

    private void prepareMediaSource(
            MediaSource source, MediaRouteSelector selector, String sourceId, String appId) {
        doReturn(sourceId).when(source).getSourceId();
        doReturn(source).when(mProvider).getSourceFromId(eq(sourceId));
        doReturn(appId).when(source).getApplicationId();
        doReturn(selector).when(source).buildRouteSelector();
    }
}
