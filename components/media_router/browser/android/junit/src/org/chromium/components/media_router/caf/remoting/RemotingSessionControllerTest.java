// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf.remoting;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyDouble;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import com.google.android.gms.cast.MediaInfo;
import com.google.android.gms.cast.framework.CastContext;
import com.google.android.gms.cast.framework.CastSession;
import com.google.android.gms.cast.framework.media.RemoteMediaClient;
import com.google.android.gms.common.api.PendingResult;

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
import org.chromium.components.media_router.MediaRouterClient;
import org.chromium.components.media_router.MediaSink;
import org.chromium.components.media_router.TestMediaRouterClient;
import org.chromium.components.media_router.caf.CreateRouteRequestInfo;
import org.chromium.components.media_router.caf.MediaRouterTestHelper;
import org.chromium.components.media_router.caf.ShadowCastContext;
import org.chromium.components.media_router.caf.ShadowMediaRouter;

/** Robolectric tests for RemotingSessionController. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowMediaRouter.class, ShadowCastContext.class})
public class RemotingSessionControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private MediaRouterTestHelper mMediaRouterHelper;
    private RemotingSessionController mController;
    private CreateRouteRequestInfo mRequestInfo;
    @Mock private CastContext mCastContext;
    @Mock private CafRemotingMediaRouteProvider mProvider;
    @Mock private CastSession mOldCastSession;
    @Mock private CastSession mNewCastSession;
    @Mock private RemoteMediaClient mOldRemoteMediaClient;
    @Mock private RemoteMediaClient mNewRemoteMediaClient;

    @Before
    public void setUp() {
        mMediaRouterHelper = new MediaRouterTestHelper();
        MediaRouterClient.setInstance(new TestMediaRouterClient());
        ShadowCastContext.setInstance(mCastContext);
        mController = new RemotingSessionController(mProvider);
        RemotingMediaSource source = mock(RemotingMediaSource.class);
        doReturn("https://example.com/video.mp4").when(source).getMediaUrl();
        mRequestInfo =
                new CreateRouteRequestInfo(
                        source,
                        mock(MediaSink.class),
                        "presentation-id",
                        "origin",
                        1,
                        false,
                        1,
                        mMediaRouterHelper.getCastRoute());
        doReturn(mRequestInfo).when(mProvider).getPendingCreateRouteRequestInfo();
        stubCastSession(mOldCastSession, mOldRemoteMediaClient);
        stubCastSession(mNewCastSession, mNewRemoteMediaClient);
    }

    @After
    public void tearDown() {
        MediaRouterClient.setInstance(null);
    }

    @Test
    public void testUpdateMediaSource() {
        mController.requestSessionLaunch();
        mController.onSessionStarted();
        assertSame(mRequestInfo, mController.getRouteCreationInfo());

        RemotingMediaSource source2 =
                RemotingMediaSource.from(
                        "remote-playback:media-element?source=123&video_codec=vp8&audio_codec=mp3");
        mController.updateMediaSource(source2);
        assertSame(mRequestInfo.getMediaSource(), source2);
        verify(mProvider).updateRouteMediaSource(mRequestInfo.routeId, source2.getSourceId());
    }

    @Test
    public void testFlingingControllerReleasedWhenSessionEnds() {
        FlingingControllerAdapter oldAdapter = startSession(mOldCastSession);

        mController.onSessionEnded();
        mController.detachFromCastSession();
        assertNull(mController.getFlingingController());

        FlingingControllerAdapter newAdapter = startSession(mNewCastSession);
        assertNotSame(oldAdapter, newAdapter);

        sendAllCommands(oldAdapter);
        verifyNoMediaCommands(mNewRemoteMediaClient);

        // The adapter of the current session still works.
        newAdapter.pause();
        verify(mNewRemoteMediaClient).pause();
    }

    @Test
    public void testFlingingControllerReleasedWhenNewSessionStarts() {
        FlingingControllerAdapter oldAdapter = startSession(mOldCastSession);

        // A new session replaces the old one without onSessionEnded().
        FlingingControllerAdapter newAdapter = startSession(mNewCastSession);
        assertNotSame(oldAdapter, newAdapter);

        sendAllCommands(oldAdapter);
        verifyNoMediaCommands(mNewRemoteMediaClient);

        newAdapter.pause();
        verify(mNewRemoteMediaClient).pause();
    }

    @Test
    public void testStatusUpdateAfterSessionEndedDoesNotCrash() {
        startSession(mOldCastSession);
        mController.onSessionEnded();
        mController.onStatusUpdated();
    }

    private FlingingControllerAdapter startSession(CastSession session) {
        mController.requestSessionLaunch();
        mController.attachToCastSession(session);
        mController.onSessionStarted();
        FlingingControllerAdapter adapter = mController.getFlingingController();
        assertNotNull(adapter);
        clearInvocations(mOldRemoteMediaClient, mNewRemoteMediaClient);
        return adapter;
    }

    private static void sendAllCommands(FlingingControllerAdapter adapter) {
        adapter.load(/* position= */ 0, /* autoplay= */ true);
        adapter.play();
        adapter.pause();
        adapter.seek(0);
        adapter.setMute(true);
        adapter.setVolume(0);
    }

    private static void verifyNoMediaCommands(RemoteMediaClient client) {
        verify(client, never()).load(any(MediaInfo.class), anyBoolean(), anyLong());
        verify(client, never()).play();
        verify(client, never()).pause();
        verify(client, never()).seek(anyLong());
        verify(client, never()).setStreamMute(anyBoolean());
        verify(client, never()).setStreamVolume(anyDouble());
    }

    @SuppressWarnings("unchecked")
    private static void stubCastSession(CastSession session, RemoteMediaClient client) {
        doReturn(true).when(session).isConnected();
        doReturn(client).when(session).getRemoteMediaClient();
        PendingResult<RemoteMediaClient.MediaChannelResult> result = mock(PendingResult.class);
        doReturn(result).when(client).play();
        doReturn(result).when(client).pause();
        doReturn(result).when(client).seek(anyLong());
        doReturn(result).when(client).setStreamMute(anyBoolean());
        doReturn(result).when(client).setStreamVolume(anyDouble());
    }
}
