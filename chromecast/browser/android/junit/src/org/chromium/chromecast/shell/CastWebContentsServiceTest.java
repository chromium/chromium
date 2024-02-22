// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Notification;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.PatternMatcher;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ServiceController;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowService;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chromecast.base.ReactiveRecorder;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.WebContents;

/**
 * Tests for CastWebContentsService.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CastWebContentsServiceTest {
    private static final String WEBCONTENTS_TITLE = "CastWebContentsServiceTest_title";

    private @Mock WebContents mWebContents;
    private @Mock MediaSession mMediaSession;
    private String mInstanceId;
    private Intent mIntent;
    private ServiceController<CastWebContentsService> mServiceLifecycle;
    private CastWebContentsService mService;
    private ShadowService mShadowService;

    private IntentFilter filterFor(String action) {
        IntentFilter filter = new IntentFilter();
        Uri instanceUri = CastWebContentsIntentUtils.getInstanceUri(mInstanceId);
        filter.addDataScheme(instanceUri.getScheme());
        filter.addDataAuthority(instanceUri.getAuthority(), null);
        filter.addDataPath(instanceUri.getPath(), PatternMatcher.PATTERN_LITERAL);
        filter.addAction(action);
        return filter;
    }

    private void expectBroadcastedIntent(IntentFilter filter, Runnable runnable) {
        BroadcastReceiver receiver = mock(BroadcastReceiver.class);
        CastWebContentsIntentUtils.getLocalBroadcastManager().registerReceiver(receiver, filter);
        try {
            runnable.run();
        } finally {
            CastWebContentsIntentUtils.getLocalBroadcastManager().unregisterReceiver(receiver);
            verify(receiver).onReceive(any(Context.class), any(Intent.class));
        }
    }

    @Before
    public void setUp() {
        mWebContents = mock(WebContents.class);
        when(mWebContents.getTitle()).thenReturn(WEBCONTENTS_TITLE);
        mMediaSession = mock(MediaSession.class);
        mInstanceId = "1";
        mIntent = CastWebContentsIntentUtils.requestStartCastService(
                RuntimeEnvironment.application, mWebContents, mInstanceId);
        mServiceLifecycle =
                Robolectric.buildService(CastWebContentsService.class).withIntent(mIntent);
        mService = mServiceLifecycle.get();
        mService.setMediaSessionGetterForTesting(
                webContents -> {
                    assertEquals(webContents, mWebContents);
                    return mMediaSession;
                });
        mShadowService = Shadows.shadowOf(mService);
    }

    @After
    public void tearDown() {
        mServiceLifecycle.unbind();
        mServiceLifecycle.destroy();
    }

    @Test
    public void testForegroundedAfterBind() {
        mServiceLifecycle.bind();
        assertNotNull(mShadowService.getLastForegroundNotification());
        assertFalse(mShadowService.isForegroundStopped());
        assertFalse(mShadowService.getNotificationShouldRemoved());
    }

    @Test
    public void testForegroundNotificationHasUniqueChannelId() {
        mServiceLifecycle.bind();
        Notification notification = mShadowService.getLastForegroundNotification();
        String notificationChannelId = notification.getChannelId();
        assertNotNull(notificationChannelId);
        assertEquals("org.chromium.chromecast.shell.CastWebContentsService.channel",
                notificationChannelId);
    }

    @Test
    public void testForegroundNotificationHasCorrectSmallIcon() {
        mServiceLifecycle.bind();
        Notification notification = mShadowService.getLastForegroundNotification();
        assertNotNull(notification.getSmallIcon());
        assertEquals(R.drawable.ic_settings_cast, notification.getSmallIcon().getResId());
    }

    @Test
    public void testNotificationTitleMatchesWebContentsTitle() {
        mServiceLifecycle.bind();
        Notification notification = mShadowService.getLastForegroundNotification();
        assertEquals(notification.extras.getCharSequence(Notification.EXTRA_TITLE).toString(),
                WEBCONTENTS_TITLE);
    }

    @Test
    public void testBackgroundedAfterUnbind() {
        mServiceLifecycle.bind().unbind();
        assertTrue(mShadowService.getNotificationShouldRemoved());
        assertTrue(mShadowService.isForegroundStopped());
    }

    @Test
    public void testBroadcastsComponentClosedWhenUnbind() {
        mServiceLifecycle.bind();
        IntentFilter filter = filterFor(CastWebContentsIntentUtils.ACTION_ACTIVITY_STOPPED);
        expectBroadcastedIntent(filter, () -> mServiceLifecycle.unbind());
    }

    @Test
    public void testDisplaysContentsOnBindAndReleasesOnUnbind() {
        ReactiveRecorder recordWebContentsPresentation =
                ReactiveRecorder.record(mService.observeWebContentsStateForTesting());
        mServiceLifecycle.bind();
        recordWebContentsPresentation.verify().opened(mWebContents).end();
        mServiceLifecycle.unbind();
        recordWebContentsPresentation.verify().closed(mWebContents).end();
    }

    @Test
    public void testDoesNotDisplayNullWebContents() {
        ReactiveRecorder recordWebContentsPresentation =
                ReactiveRecorder.record(mService.observeWebContentsStateForTesting());
        mIntent = CastWebContentsIntentUtils.requestStartCastService(
                RuntimeEnvironment.application, null, mInstanceId);
        mServiceLifecycle.withIntent(mIntent).bind();
        recordWebContentsPresentation.verify().end();
    }

    @Test
    public void testRequestsSystemAudioFocusOnBind() {
        mServiceLifecycle.bind();
        verify(mMediaSession).requestSystemAudioFocus();
    }
}
