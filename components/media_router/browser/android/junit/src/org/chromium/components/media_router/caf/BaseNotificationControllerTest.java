// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Intent;

import com.google.android.gms.cast.CastDevice;
import com.google.android.gms.cast.framework.CastSession;
import com.google.android.gms.cast.framework.media.RemoteMediaClient;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.components.media_router.MediaRouterClient;
import org.chromium.components.media_router.MediaSink;
import org.chromium.components.media_router.MediaSource;
import org.chromium.components.media_router.TestMediaRouterClient;

/** Robolectric tests for BaseNotificationController. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BaseNotificationControllerTest {
    private static final String PRESENTATION_ID = "presentation-id";
    private static final String ORIGIN = "https://example.com/";
    private static final int TAB_ID = 1;
    private static final String DEVICE_NAME = "My Device";

    @Mock private CastDevice mCastDevice;
    @Mock private BaseSessionController mSessionController;
    @Mock private MediaSource mSource;
    @Mock private MediaSink mSink;
    @Mock private CastSession mCastSession;
    @Mock private RemoteMediaClient mRemoteMediaClient;
    @Mock private TestMediaRouterClient mMediaRouterClient;
    private BaseNotificationController mController;
    private CreateRouteRequestInfo mRequestInfo;
    private MediaRouterTestHelper mMediaRouterHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMediaRouterHelper = new MediaRouterTestHelper();
        MediaRouterClient.setInstance(mMediaRouterClient);
        mController = new TestNotificationController(mSessionController);
        mRequestInfo =
                new CreateRouteRequestInfo(
                        mSource,
                        mSink,
                        PRESENTATION_ID,
                        ORIGIN,
                        TAB_ID,
                        false,
                        1,
                        mMediaRouterHelper.getCastRoute());

        doReturn(DEVICE_NAME).when(mCastDevice).getFriendlyName();
        doReturn(mCastDevice).when(mCastSession).getCastDevice();
        doReturn(mRemoteMediaClient).when(mSessionController).getRemoteMediaClient();
        doReturn(true).when(mSessionController).isConnected();
        doReturn(mRequestInfo).when(mSessionController).getRouteCreationInfo();
        doReturn(mCastSession).when(mSessionController).getSession();
    }

    @After
    public void tearDown() {
        MediaRouterClient.setInstance(null);
    }

    @Test
    public void testOnSessionStarted() {
        ArgumentCaptor<MediaNotificationInfo> notificationInfoCaptor =
                ArgumentCaptor.forClass(MediaNotificationInfo.class);
        mController.onSessionStarted();

        verify(mMediaRouterClient).showNotification(notificationInfoCaptor.capture());
        MediaNotificationInfo notificationInfo = notificationInfoCaptor.getValue();
        assertEquals(DEVICE_NAME, notificationInfo.metadata.getTitle());
    }

    private static class TestNotificationController extends BaseNotificationController {
        public TestNotificationController(BaseSessionController sessionController) {
            super(sessionController);
        }

        @Override
        public Intent createContentIntent() {
            return null;
        }

        @Override
        public int getNotificationId() {
            return 42;
        }
    }
}
