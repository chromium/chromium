// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.mediarouter.media.MediaRouter;

import com.google.android.gms.cast.framework.CastContext;
import com.google.android.gms.cast.framework.CastSession;
import com.google.android.gms.cast.framework.SessionManager;
import com.google.android.gms.cast.framework.media.RemoteMediaClient;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.media_router.ClientRecord;
import org.chromium.components.media_router.MediaRoute;
import org.chromium.components.media_router.MediaRouteManager;
import org.chromium.components.media_router.MediaRouterClient;
import org.chromium.components.media_router.MediaSink;
import org.chromium.components.media_router.TestMediaRouterClient;

/** Robolectric tests for CafMediaRouteProvider. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {
            ShadowMediaRouter.class,
            ShadowCastContext.class,
            ShadowLooper.class,
            ShadowCastMediaSource.class
        })
public class CafMediaRouteProviderTest {
    private Context mContext;
    private CafMediaRouteProvider mProvider;
    private MediaRouterTestHelper mMediaRouterHelper;
    private MediaRouter mMediaRouter;
    private MediaRoute mRoute1;
    private MediaRoute mRoute2;

    @Mock private MediaRouteManager mManager;
    @Mock private CastContext mCastContext;
    @Mock private CastSession mCastSession;
    @Mock private SessionManager mSessionManager;
    @Mock private RemoteMediaClient mRemoteMediaClient;
    @Mock private BaseSessionController mSessionController;
    @Mock private ShadowCastMediaSource.ShadowImplementation mShadowCastMediaSource;
    @Mock private CafMessageHandler mMessageHandler;
    @Mock private CastMediaSource mSource1;
    @Mock private CastMediaSource mSource2;
    @Mock private MediaSink mSink;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        MediaRouterClient.setInstance(new TestMediaRouterClient());

        mContext = RuntimeEnvironment.application;
        ShadowCastContext.setInstance(mCastContext);
        ShadowCastMediaSource.setImplementation(mShadowCastMediaSource);
        mMediaRouterHelper = new MediaRouterTestHelper();
        mMediaRouter = MediaRouter.getInstance(mContext);
        mProvider = spy(CafMediaRouteProvider.create(mManager));
        mProvider.mMessageHandler = mMessageHandler;

        mRoute1 = new MediaRoute("sink-id", "source-id-1", "presentation-id-1");
        mRoute2 = new MediaRoute("sink-id", "source-id-2", "presentation-id-2");
        doReturn(mSource1).when(mShadowCastMediaSource).from("source-id-1");
        doReturn(mSource2).when(mShadowCastMediaSource).from("source-id-2");
        doReturn("client-id-1").when(mSource1).getClientId();
        doReturn("client-id-2").when(mSource2).getClientId();
        doReturn("app-id-1").when(mSource1).getApplicationId();
        doReturn("app-id-2").when(mSource2).getApplicationId();
        doReturn("sink-id").when(mSink).getId();
        doReturn(mSessionController).when(mProvider).sessionController();
        doReturn(mSessionManager).when(mCastContext).getSessionManager();
        doReturn(mCastSession).when(mSessionController).getSession();
        doReturn(mRemoteMediaClient).when(mCastSession).getRemoteMediaClient();
    }

    @After
    public void tearDown() {
        MediaRouterClient.setInstance(null);
    }

    @Test
    public void testJoinRoute() {
        InOrder inOrder = inOrder(mManager);

        doReturn(mSource1).when(mShadowCastMediaSource).from("source-id-1");
        doReturn(mSink).when(mSessionController).getSink();
        doReturn(true).when(mSessionController).isConnected();
        doReturn(true)
                .when(mProvider)
                .canJoinExistingSession(
                        anyString(), anyString(), anyInt(), any(CastMediaSource.class));

        // Regular case.
        mProvider.joinRoute("source-id-1", "presentation-id-1", "origin", 1, 1);
        inOrder.verify(mManager, never()).onJoinRouteRequestError(anyString(), anyInt());
        inOrder.verify(mManager)
                .onRouteCreated(anyString(), eq("sink-id"), eq(1), eq(mProvider), eq(false));
        assertEquals(mProvider.mRoutes.size(), 1);
        MediaRoute route = (MediaRoute) mProvider.mRoutes.values().toArray()[0];
        assertEquals(route.sinkId, "sink-id");
        assertEquals(route.getSourceId(), "source-id-1");
        assertEquals(route.presentationId, "presentation-id-1");

        // No source.
        mProvider.mRoutes.clear();
        doReturn(null).when(mShadowCastMediaSource).from("source-id-1");

        mProvider.joinRoute("source-id-1", "presentation-id-1", "origin", 1, 1);

        verifyJoinRouteRequestError(inOrder, "Unsupported presentation URL", 1);
        assertTrue(mProvider.mRoutes.isEmpty());

        // No client ID.
        doReturn(mSource1).when(mShadowCastMediaSource).from("source-id-1");
        doReturn(null).when(mSource1).getClientId();

        mProvider.joinRoute("source-id-1", "presentation-id-1", "origin", 1, 1);

        verifyJoinRouteRequestError(inOrder, "Unsupported presentation URL", 1);
        assertTrue(mProvider.mRoutes.isEmpty());

        // No session.
        doReturn("client-id-1").when(mSource1).getClientId();
        doReturn(false).when(mSessionController).isConnected();

        mProvider.joinRoute("source-id-1", "presentation-id-1", "origin", 1, 1);

        verifyJoinRouteRequestError(inOrder, "No presentation", 1);
        assertTrue(mProvider.mRoutes.isEmpty());

        // No matching route.
        doReturn(true).when(mSessionController).isConnected();
        doReturn(false)
                .when(mProvider)
                .canJoinExistingSession(
                        anyString(), anyString(), anyInt(), any(CastMediaSource.class));

        mProvider.joinRoute("source-id-1", "presentation-id-1", "origin", 1, 1);

        verifyJoinRouteRequestError(inOrder, "No matching route", 1);
        assertTrue(mProvider.mRoutes.isEmpty());
    }

    @Test
    public void testCloseRoute() {
        InOrder inOrder = inOrder(mMessageHandler);

        doReturn(mSink).when(mSessionController).getSink();

        // Regular case when there is active session.
        mProvider.addRoute(mRoute1, "origin", 1, 1, false);
        doReturn(true).when(mSessionController).isConnected();

        mProvider.closeRoute(mRoute1.id);

        inOrder.verify(mMessageHandler)
                .sendReceiverActionToClient(mRoute1.id, mSink, "client-id-1", "stop");
        assertEquals(mProvider.mRoutes.size(), 1);
        assertEquals(mProvider.getClientIdToRecords().size(), 1);

        // Abnormal case when the session controller doesn't have a sink.
        doReturn(null).when(mSessionController).getSink();

        mProvider.closeRoute(mRoute1.id);

        inOrder.verify(mMessageHandler, never())
                .sendReceiverActionToClient(
                        anyString(), any(MediaSink.class), anyString(), anyString());
        assertEquals(mProvider.mRoutes.size(), 1);
        assertEquals(mProvider.getClientIdToRecords().size(), 1);

        // Abnormal case when there is no session.
        doReturn(mSink).when(mSessionController).getSink();
        doReturn(false).when(mSessionController).isConnected();

        mProvider.closeRoute(mRoute1.id);

        inOrder.verify(mMessageHandler, never())
                .sendReceiverActionToClient(
                        anyString(), any(MediaSink.class), anyString(), anyString());
        assertTrue(mProvider.mRoutes.isEmpty());
        assertTrue(mProvider.getClientIdToRecords().isEmpty());
    }

    @Test
    public void testSendStringMessage() {
        InOrder inOrder = inOrder(mMessageHandler);

        mProvider.addRoute(mRoute1, "origin", 1, 1, false);

        // A client in record sends a message.
        mProvider.sendStringMessage(mRoute1.id, "message");
        inOrder.verify(mMessageHandler).handleMessageFromClient("message");

        // An unknown client sends a mesasge.
        mProvider.sendStringMessage("other-route-id", "message");
        inOrder.verify(mMessageHandler, never()).handleMessageFromClient(anyString());
    }

    @Test
    public void testSendMessageToClient() {
        InOrder inOrder = inOrder(mManager);

        mProvider.addRoute(mRoute1, "origin", 1, 1, false);
        mProvider.getClientIdToRecords().get("client-id-1").isConnected = true;

        // Normal case.
        mProvider.sendMessageToClient("client-id-1", "message");
        inOrder.verify(mManager).onMessage(mRoute1.id, "message");

        // Client is not in record.
        mProvider.sendMessageToClient("client-id-unkonwn", "message");
        inOrder.verify(mManager, never()).onMessage(anyString(), anyString());

        // Message enqueued while client is not connected.
        mProvider.getClientIdToRecords().get("client-id-1").isConnected = false;
        mProvider.sendMessageToClient("client-id-1", "message");
        inOrder.verify(mManager, never()).onMessage(anyString(), anyString());

        // Flush message
        mProvider.flushPendingMessagesToClient(mProvider.getClientIdToRecords().get("client-id-1"));
        inOrder.verify(mManager).onMessage(mRoute1.id, "message");
    }

    @Test
    public void testOnSessionStarted() {
        InOrder inOrder = inOrder(mSessionController, mRemoteMediaClient);

        doReturn(mSink).when(mSessionController).getSink();
        doReturn(mCastSession).when(mSessionManager).getCurrentCastSession();
        doReturn(null).when(mSessionController).getSession();
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                doReturn(invocation.getArguments()[0])
                                        .when(mSessionController)
                                        .getSession();
                                return null;
                            }
                        })
                .when(mSessionController)
                .attachToCastSession(any(CastSession.class));

        // Prepare the pending create route request so super.onSessionStarted() behaves correctly.
        mProvider.createRoute(
                "source-id-1", "cast-route", "presentation-id", "origin", 1, false, 1);

        mProvider.addRoute(mRoute1, "origin", 1, 1, false);
        mProvider.addRoute(mRoute2, "origin", 1, 1, false);

        // Skip adding route when the super.onSessionStarted() is called.
        doNothing()
                .when(mProvider)
                .addRoute(any(MediaRoute.class), anyString(), anyInt(), anyInt(), anyBoolean());
        mProvider.onSessionStarted(mCastSession, "session-id");

        // Verify super.onSessionStarted() is called.
        inOrder.verify(mSessionController).attachToCastSession(mCastSession);
        inOrder.verify(mRemoteMediaClient).requestStatus();
        verify(mMessageHandler)
                .sendReceiverActionToClient(mRoute1.id, mSink, "client-id-1", "cast");
        verify(mMessageHandler)
                .sendReceiverActionToClient(mRoute2.id, mSink, "client-id-2", "cast");
    }

    @Test
    public void testRouteManagement() {
        // Add the first route.
        mProvider.addRoute(mRoute1, "origin-1", 1, 1, false);
        assertEquals(mProvider.mRoutes.size(), 1);
        assertEquals(mProvider.getClientIdToRecords().size(), 1);
        ClientRecord record = mProvider.getClientIdToRecords().get("client-id-1");
        verifyClientRecord(record, mRoute1.id, "client-id-1", "app-id-1", "origin-1", 1, false);

        // Add the second route.
        mProvider.addRoute(mRoute2, "origin-2", 2, 2, false);
        assertEquals(mProvider.mRoutes.size(), 2);
        assertEquals(mProvider.getClientIdToRecords().size(), 2);
        record = mProvider.getClientIdToRecords().get("client-id-2");
        verifyClientRecord(record, mRoute2.id, "client-id-2", "app-id-2", "origin-2", 2, false);

        // Add a duplicate route. This addition will be ignored as `mRoute2` is already in record.
        // This should never happen in production.
        mProvider.addRoute(mRoute2, "origin-3", 3, 3, false);
        assertEquals(mProvider.mRoutes.size(), 2);
        assertEquals(mProvider.getClientIdToRecords().size(), 2);
        record = mProvider.getClientIdToRecords().get("client-id-2");
        verifyClientRecord(record, mRoute2.id, "client-id-2", "app-id-2", "origin-2", 2, false);

        // Remove a route.
        ClientRecord lastRecord = mProvider.getClientIdToRecords().get("client-id-1");
        mProvider.removeRoute(mRoute1.id, null);
        assertEquals(mProvider.mRoutes.size(), 1);
        assertEquals(mProvider.getClientIdToRecords().size(), 1);
        record = mProvider.getClientIdToRecords().get("client-id-2");
        verifyClientRecord(record, mRoute2.id, "client-id-2", "app-id-2", "origin-2", 2, false);
        assertEquals(mProvider.mLastRemovedRouteRecord, lastRecord);

        // Remove a non-existing route.
        mProvider.removeRoute(mRoute1.id, null);
        assertEquals(mProvider.mRoutes.size(), 1);
        assertEquals(mProvider.getClientIdToRecords().size(), 1);
        record = mProvider.getClientIdToRecords().get("client-id-2");
        verifyClientRecord(record, mRoute2.id, "client-id-2", "app-id-2", "origin-2", 2, false);
        lastRecord = record;

        // Remove the last route.
        mProvider.removeRoute(mRoute2.id, null);
        assertTrue(mProvider.mRoutes.isEmpty());
        assertTrue(mProvider.getClientIdToRecords().isEmpty());
        assertEquals(mProvider.mLastRemovedRouteRecord, lastRecord);
    }

    @Test
    public void testCanJoin_matchingSessionId() {
        // Regular case.
        doReturn("session-id").when(mSessionController).getSessionId();
        assertTrue(
                mProvider.canJoinExistingSession(
                        "cast-session_session-id", "origin", 1, mock(CastMediaSource.class)));

        // The current session ID is null.
        doReturn(null).when(mSessionController).getSessionId();
        assertFalse(
                mProvider.canJoinExistingSession(
                        "cast-session_session-id", "origin", 1, mock(CastMediaSource.class)));

        // Mismatching session ID.
        doReturn("session-id").when(mSessionController).getSessionId();
        assertFalse(
                mProvider.canJoinExistingSession(
                        "cast-session_other-session-id", "origin", 1, mock(CastMediaSource.class)));
    }

    @Test
    public void testAutoJoin_usingLastRemovedRouteRecord() {
        doReturn("app-id-1").when(mSource1).getApplicationId();
        doReturn("app-id-1").when(mSource2).getApplicationId();
        doReturn("tab_and_origin_scoped").when(mSource2).getAutoJoinPolicy();
        doReturn(mSource1).when(mSessionController).getSource();

        mProvider.addRoute(mRoute1, "origin-1", 1, 1, false);
        mProvider.removeRoute(mRoute1.id, null);

        // Regular case.
        assertTrue(mProvider.canJoinExistingSession("auto-join", "origin-1", 1, mSource2));

        // Mismatching origin.
        assertFalse(mProvider.canJoinExistingSession("auto-join", "origin-2", 1, mSource2));

        // Mismatching tab id.
        assertFalse(mProvider.canJoinExistingSession("auto-join", "origin-1", 2, mSource2));
    }

    @Test
    public void testAutoJoin_mismatchingSources() {
        doReturn("app-id-1").when(mSource1).getApplicationId();
        doReturn("app-id-1").when(mSource2).getApplicationId();
        doReturn("tab_and_origin_scoped").when(mSource2).getAutoJoinPolicy();
        doReturn(mSource1).when(mSessionController).getSource();

        mProvider.addRoute(mRoute1, "origin-1", 1, 1, false);
        mProvider.removeRoute(mRoute1.id, null);

        // Page scoped auto-join policy.
        doReturn("page_scoped").when(mSource2).getAutoJoinPolicy();
        assertFalse(mProvider.canJoinExistingSession("auto-join", "origin-1", 1, mSource2));

        // Mismatching app ID.
        doReturn("tab_and_origin_scoped").when(mSource2).getAutoJoinPolicy();
        doReturn("app-id-2").when(mSource2).getApplicationId();
        assertFalse(mProvider.canJoinExistingSession("auto-join", "origin-1", 1, mSource2));
    }

    @Test
    public void testAutoJoin_originScoped() {
        doReturn("app-id-1").when(mSource1).getApplicationId();
        doReturn("app-id-1").when(mSource2).getApplicationId();
        doReturn("origin_scoped").when(mSource2).getAutoJoinPolicy();
        doReturn(mSource1).when(mSessionController).getSource();

        mProvider.addRoute(mRoute1, "origin-1", 1, 1, false);

        // Normal case.
        assertTrue(mProvider.canJoinExistingSession("auto-join", "origin-1", 1, mSource2));

        // Mismatching tab ID is allowed.
        assertTrue(mProvider.canJoinExistingSession("auto-join", "origin-1", 2, mSource2));

        // Mismatching origin is not allowed.
        assertFalse(mProvider.canJoinExistingSession("auto-join", "origin-2", 1, mSource2));
    }

    @Test
    public void testAutoJoin_tabAndOriginScoped() {
        doReturn("app-id-1").when(mSource1).getApplicationId();
        doReturn("app-id-1").when(mSource2).getApplicationId();
        doReturn("tab_and_origin_scoped").when(mSource2).getAutoJoinPolicy();
        doReturn(mSource1).when(mSessionController).getSource();

        mProvider.addRoute(mRoute1, "origin-1", 1, 1, false);

        // Normal case.
        assertTrue(mProvider.canJoinExistingSession("auto-join", "origin-1", 1, mSource2));

        // Mismatching tab ID is not allowed.
        assertFalse(mProvider.canJoinExistingSession("auto-join", "origin-1", 2, mSource2));

        // Mismatching origin is not allowed.
        assertFalse(mProvider.canJoinExistingSession("auto-join", "origin-2", 1, mSource2));
    }

    private void verifyJoinRouteRequestError(InOrder inOrder, String error, int nativeRequestId) {
        inOrder.verify(mManager).onJoinRouteRequestError(error, nativeRequestId);
        inOrder.verify(mManager, never())
                .onRouteCreated(
                        anyString(),
                        anyString(),
                        anyInt(),
                        any(CafBaseMediaRouteProvider.class),
                        anyBoolean());
    }

    private void verifyClientRecord(
            ClientRecord record,
            String routeId,
            String clientId,
            String appId,
            String origin,
            int tabId,
            boolean isConnected) {
        assertEquals(record.routeId, routeId);
        assertEquals(record.clientId, clientId);
        assertEquals(record.appId, appId);
        assertEquals(record.origin, origin);
        assertEquals(record.tabId, tabId);
        assertEquals(record.isConnected, isConnected);
    }
}
