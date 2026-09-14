// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Notification;
import android.content.Intent;
import android.os.Looper;
import android.support.v4.media.session.MediaSessionCompat;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.ForegroundServiceUtils;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.services.media_session.MediaMetadata;

/**
 * Tests for {@link MediaNotificationController} in service-less mode (see {@link
 * MediaNotificationController.Delegate#useForegroundService()}), as used by WebView: the
 * notification is posted directly through the notification manager proxy instead of being attached
 * to a foreground service, and action intents are delivered via broadcasts.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class MediaNotificationControllerServicelessTest {
    // Kept identical for notification id, MediaNotificationInfo.id, and media type id, matching
    // the single-notification configuration where the manager doesn't allocate unique ids.
    private static final int NOTIFICATION_ID = 100;
    private static final int TAB_ID = 42;

    private MockNotificationManagerProxy mMockProxy;
    private ForegroundServiceUtils mForegroundServiceUtils;
    private MediaNotificationListener mListener;
    private MediaNotificationController mController;

    /** A Delegate configured for service-less mode. */
    private static class ServicelessDelegate implements MediaNotificationController.Delegate {
        @Override
        public boolean useForegroundService() {
            return false;
        }

        @Override
        public @Nullable Intent createServiceIntent() {
            return null;
        }

        @Override
        public String getAppName() {
            return "TestApp";
        }

        @Override
        public String getNotificationGroupName() {
            return "TestGroup";
        }

        @Override
        public NotificationWrapperBuilder createNotificationWrapperBuilder() {
            NotificationWrapperBuilder builder = mock(NotificationWrapperBuilder.class);
            when(builder.buildNotificationWrapper())
                    .thenReturn(
                            new NotificationWrapper(
                                    new Notification(),
                                    new NotificationMetadata(
                                            /* notificationType= */ 0,
                                            /* notificationTag= */ null,
                                            NOTIFICATION_ID)));
            return builder;
        }

        @Override
        public void onMediaSessionUpdated(MediaSessionCompat session) {}

        @Override
        public void logNotificationShown(NotificationWrapper notification) {}

        @Override
        public int getNotificationId() {
            return NOTIFICATION_ID;
        }

        @Override
        public int getMediaTypeId() {
            return NOTIFICATION_ID;
        }
    }

    @Before
    public void setUp() {
        mMockProxy = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mMockProxy);

        mForegroundServiceUtils = mock(ForegroundServiceUtils.class);
        ForegroundServiceUtils.setInstanceForTesting(mForegroundServiceUtils);

        mListener = mock(MediaNotificationListener.class);

        mController = new MediaNotificationController(new ServicelessDelegate());
        // Normally created lazily from the UI-thread idle handler; the tests call
        // showNotification() directly, which requires it to already exist.
        mController.mPendingIntentInitializer.createPendingIntentActionSwipeIfNeeded();
    }

    private MediaNotificationInfo.Builder createInfoBuilder(boolean isPaused) {
        return new MediaNotificationInfo.Builder()
                .setMetadata(new MediaMetadata("title", "artist", "album"))
                .setOrigin("https://example.com")
                .setInstanceId(TAB_ID)
                .setId(NOTIFICATION_ID)
                .setPaused(isPaused)
                .setListener(mListener);
    }

    private void sendActionBroadcast(String action, int targetNotificationId) {
        Intent intent =
                new Intent(action)
                        .setPackage(ContextUtils.getApplicationContext().getPackageName())
                        .putExtra(
                                MediaNotificationController.EXTRA_NOTIFICATION_ID,
                                targetNotificationId);
        ContextUtils.getApplicationContext().sendBroadcast(intent);
        shadowOf(Looper.getMainLooper()).idle();
    }

    @Test
    public void testShowPostsNotificationWithoutForegroundService() {
        mController.showNotification(createInfoBuilder(/* isPaused= */ false).build());

        assertEquals(1, mMockProxy.getNotifications().size());
        assertEquals(NOTIFICATION_ID, mMockProxy.getNotifications().get(0).getId());
        // The whole point of service-less mode: the foreground service machinery is never used.
        verifyNoInteractions(mForegroundServiceUtils);
    }

    @Test
    public void testInitiallyPausedMediaShowsNoNotification() {
        mController.showNotification(createInfoBuilder(/* isPaused= */ true).build());

        assertTrue(mMockProxy.getNotifications().isEmpty());
    }

    @Test
    public void testPauseAfterShowingKeepsNotification() {
        mController.showNotification(createInfoBuilder(/* isPaused= */ false).build());
        mController.showNotification(createInfoBuilder(/* isPaused= */ true).build());

        // The paused update replaces the notification rather than removing it, so the user can
        // resume or dismiss it.
        assertEquals(1, mMockProxy.getNotifications().size());
    }

    @Test
    public void testClearNotificationCancels() {
        mController.showNotification(createInfoBuilder(/* isPaused= */ false).build());
        mController.clearNotification();

        assertTrue(mMockProxy.getNotifications().isEmpty());
    }

    @Test
    public void testActionBroadcastReachesListener() {
        mController.showNotification(createInfoBuilder(/* isPaused= */ false).build());

        sendActionBroadcast(MediaNotificationController.ACTION_PAUSE, NOTIFICATION_ID);

        verify(mListener).onPause(MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION);
    }

    @Test
    public void testSwipeBroadcastStopsPlayback() {
        mController.showNotification(createInfoBuilder(/* isPaused= */ false).build());

        sendActionBroadcast(MediaNotificationController.ACTION_SWIPE, NOTIFICATION_ID);

        verify(mListener).onStop(MediaNotificationListener.ACTION_SOURCE_MEDIA_NOTIFICATION);
    }

    @Test
    public void testBroadcastForOtherControllerIsIgnored() {
        mController.showNotification(createInfoBuilder(/* isPaused= */ false).build());

        sendActionBroadcast(MediaNotificationController.ACTION_PAUSE, NOTIFICATION_ID + 1);

        verify(mListener, never()).onPause(anyInt());
    }

    @Test
    public void testNoBroadcastsHandledAfterClear() {
        mController.showNotification(createInfoBuilder(/* isPaused= */ false).build());
        mController.clearNotification();

        sendActionBroadcast(MediaNotificationController.ACTION_PAUSE, NOTIFICATION_ID);

        verify(mListener, never()).onPause(anyInt());
    }
}
