// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.content.Context;

import com.google.android.gms.cast.ApplicationMetadata;
import com.google.android.gms.cast.Cast;
import com.google.android.gms.cast.CastDevice;
import com.google.android.gms.cast.framework.CastSession;
import com.google.android.gms.cast.framework.media.RemoteMediaClient;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.media_router.CastSessionUtil;
import org.chromium.components.media_router.MediaRouterClient;
import org.chromium.components.media_router.MediaSink;
import org.chromium.components.media_router.MediaSource;
import org.chromium.components.media_router.TestMediaRouterClient;

import java.util.ArrayList;
import java.util.List;

/** Robolectric tests for CastSessionController. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CastSessionControllerTest {
    @Mock private CastDevice mCastDevice;
    @Mock private CafMediaRouteProvider mProvider;
    @Mock private MediaSource mSource;
    @Mock private MediaSink mSink;
    @Mock private CastSession mCastSession;
    @Mock private RemoteMediaClient mRemoteMediaClient;
    @Mock private CafMessageHandler mMessageHandler;
    @Mock private ApplicationMetadata mApplicationMetadata;
    private CastSessionController mController;
    private CreateRouteRequestInfo mRequestInfo;
    private MediaRouterTestHelper mMediaRouterHelper;
    private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        MediaRouterClient.setInstance(new TestMediaRouterClient());

        mContext = RuntimeEnvironment.application;
        mMediaRouterHelper = new MediaRouterTestHelper();
        mController = spy(new CastSessionController(mProvider));
        mController.initNestedFieldsForTesting();

        doReturn(mRemoteMediaClient).when(mCastSession).getRemoteMediaClient();
        doReturn(true).when(mCastSession).isConnected();
        doReturn(mApplicationMetadata).when(mCastSession).getApplicationMetadata();
        doReturn(mCastDevice).when(mCastSession).getCastDevice();
        doReturn(mMessageHandler).when(mProvider).getMessageHandler();
        doReturn("session_message").when(mMessageHandler).buildSessionMessage();
    }

    @After
    public void tearDown() {
        MediaRouterClient.setInstance(null);
    }

    @Test
    public void testSessionAttachment() {
        // Attaching to a session.
        mController.attachToCastSession(mCastSession);
        verify(mCastSession).addCastListener(any(Cast.Listener.class));
        assertSame(mController.getSession(), mCastSession);

        // Detaching from a session.
        mController.detachFromCastSession();
        verify(mCastSession).removeCastListener(any(Cast.Listener.class));
        assertNull(mController.getSession());
    }

    @Test
    public void testOnSessionEnded() {
        mController.attachToCastSession(mCastSession);
        mController.onSessionEnded();
        verify(mMessageHandler).onSessionEnded();
    }

    @Test
    public void testCastListener() {
        InOrder inOrder = inOrder(mController, mMessageHandler);
        ArgumentCaptor<Cast.Listener> castListenerCaptor =
                ArgumentCaptor.forClass(Cast.Listener.class);

        mController.attachToCastSession(mCastSession);
        verify(mCastSession).addCastListener(castListenerCaptor.capture());

        // When the application status is changed, the namespaces should be updated and a session
        // message needs to be broadcasted.
        castListenerCaptor.getValue().onApplicationStatusChanged();

        inOrder.verify(mController).updateNamespaces();
        inOrder.verify(mMessageHandler).broadcastClientMessage("update_session", "session_message");

        // When the application metadata is changed, the namespaces should be updated and a session
        // message needs to be broadcasted.
        castListenerCaptor.getValue().onApplicationMetadataChanged(mApplicationMetadata);

        inOrder.verify(mController).updateNamespaces();
        inOrder.verify(mMessageHandler).broadcastClientMessage("update_session", "session_message");

        // When the volume is changed, the namespaces should be updated and a session message and a
        // volume change message needs to be broadcasted.
        castListenerCaptor.getValue().onVolumeChanged();

        inOrder.verify(mController).updateNamespaces();
        inOrder.verify(mMessageHandler).broadcastClientMessage("update_session", "session_message");
        inOrder.verify(mMessageHandler).onVolumeChanged();
    }

    @Test
    public void testUpdateNamespaces() throws Exception {
        org.robolectric.shadows.ShadowLog.stream = System.out;
        InOrder inOrder = inOrder(mCastSession);

        mController.attachToCastSession(mCastSession);

        List<String> namespaces = new ArrayList<>();
        List<String> observedNamespaces = new ArrayList<>();
        doReturn(namespaces).when(mApplicationMetadata).getSupportedNamespaces();
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                observedNamespaces.add((String) invocation.getArguments()[0]);
                                return null;
                            }
                        })
                .when(mCastSession)
                .setMessageReceivedCallbacks(
                        any(String.class), any(Cast.MessageReceivedCallback.class));
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                observedNamespaces.remove((String) invocation.getArguments()[0]);
                                return null;
                            }
                        })
                .when(mCastSession)
                .removeMessageReceivedCallbacks(any(String.class));

        // New namespaces added.
        namespaces.add("namespace1");
        namespaces.add("namespace2");

        mController.updateNamespaces();

        assertNamespacesContainsExactly(observedNamespaces, "namespace1", "namespace2");

        // Namespaces unchanged.
        mController.updateNamespaces();

        assertNamespacesContainsExactly(observedNamespaces, "namespace1", "namespace2");

        // New namespace added.
        namespaces.add("namespace3");

        mController.updateNamespaces();

        assertNamespacesContainsExactly(
                observedNamespaces, "namespace1", "namespace2", "namespace3");
        // Namespaces removed.
        namespaces.remove("namespace1");
        namespaces.remove("namespace3");

        mController.updateNamespaces();

        assertNamespacesContainsExactly(observedNamespaces, "namespace2");
    }

    @Test
    public void testOnMessageReceived() {
        mController.attachToCastSession(mCastSession);

        // Non-media namespaces should be forwarded to the message handler. RemoteMediaClient only
        // listens to the media namespace thus it shouldn't be notified.
        mController.onMessageReceived(mCastDevice, "namespace", "message");
        verify(mRemoteMediaClient, never())
                .onMessageReceived(any(CastDevice.class), anyString(), anyString());
        verify(mMessageHandler).onMessageReceived("namespace", "message");

        // Media namespaces should be both forwarded to the message handler and RemoteMediaClient.
        mController.onMessageReceived(mCastDevice, CastSessionUtil.MEDIA_NAMESPACE, "message");
        verify(mRemoteMediaClient)
                .onMessageReceived(mCastDevice, CastSessionUtil.MEDIA_NAMESPACE, "message");
        verify(mMessageHandler).onMessageReceived(CastSessionUtil.MEDIA_NAMESPACE, "message");
    }

    private void assertNamespacesContainsExactly(
            List<String> namespaces, String... expectedNamespaces) {
        assertEquals(namespaces.size(), expectedNamespaces.length);
        for (String namespace : expectedNamespaces) {
            assertTrue(namespaces.contains(namespace));
        }
    }
}
