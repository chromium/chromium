// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf.remoting;

import static org.junit.Assert.assertSame;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import com.google.android.gms.cast.framework.CastContext;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
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
@Config(
        manifest = Config.NONE,
        shadows = {ShadowMediaRouter.class, ShadowCastContext.class})
public class RemotingSessionControllerTest {
    private MediaRouterTestHelper mMediaRouterHelper;
    private RemotingSessionController mController;
    private CreateRouteRequestInfo mRequestInfo;
    @Mock private CastContext mCastContext;
    @Mock private CafRemotingMediaRouteProvider mProvider;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMediaRouterHelper = new MediaRouterTestHelper();
        MediaRouterClient.setInstance(new TestMediaRouterClient());
        ShadowCastContext.setInstance(mCastContext);
        mController = new RemotingSessionController(mProvider);
        mRequestInfo =
                new CreateRouteRequestInfo(
                        mock(RemotingMediaSource.class),
                        mock(MediaSink.class),
                        "presentation-id",
                        "origin",
                        1,
                        false,
                        1,
                        mMediaRouterHelper.getCastRoute());
        doReturn(mRequestInfo).when(mProvider).getPendingCreateRouteRequestInfo();
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
}
