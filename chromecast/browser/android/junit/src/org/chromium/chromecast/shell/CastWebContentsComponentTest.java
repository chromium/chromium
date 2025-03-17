// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;
import androidx.test.core.app.ApplicationProvider;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chromecast.shell.CastWebContentsComponent.StartParams;
import org.chromium.content_public.browser.WebContents;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;

/** Tests for CastWebContentsComponent. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CastWebContentsComponentTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String APP_ID = "app";

    private static final String SESSION_ID = "123456789";

    private @Mock WebContents mWebContents;
    private Application mApplication;
    private ShadowApplication mShadowApplication;
    private StartParams mStartParams;

    @Captor private ArgumentCaptor<Intent> mIntentCaptor;

    @Before
    public void setUp() {
        mApplication = ApplicationProvider.getApplicationContext();
        ContextUtils.initApplicationContextForTests(mApplication);
        mShadowApplication = Shadows.shadowOf(mApplication);
        mStartParams = new StartParams(mWebContents, APP_ID, false);
    }

    @Test
    public void testStartStartsWebContentsActivity() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);
        component.start(mStartParams);
        Intent intent = mShadowApplication.getNextStartedActivity();
        Assert.assertEquals(
                intent.getComponent().getClassName(), CastWebContentsActivity.class.getName());

        component.stop();
    }

    @Test
    public void testStopSendsStopSignalToActivity() {
        BroadcastReceiver receiver = Mockito.mock(BroadcastReceiver.class);
        IntentFilter intentFilter = new IntentFilter(CastIntents.ACTION_STOP_WEB_CONTENT);
        LocalBroadcastManager.getInstance(ContextUtils.getApplicationContext())
                .registerReceiver(receiver, intentFilter);

        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);
        component.start(mStartParams);
        component.stop();

        LocalBroadcastManager.getInstance(ContextUtils.getApplicationContext())
                .unregisterReceiver(receiver);

        verify(receiver).onReceive(any(Context.class), any(Intent.class));
    }

    @Test
    public void testEnableTouchInputSendsEnableTouchToActivity() {
        BroadcastReceiver receiver = Mockito.mock(BroadcastReceiver.class);
        IntentFilter intentFilter =
                new IntentFilter(CastWebContentsIntentUtils.ACTION_ENABLE_TOUCH_INPUT);
        LocalBroadcastManager.getInstance(ContextUtils.getApplicationContext())
                .registerReceiver(receiver, intentFilter);

        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);
        component.enableTouchInput(true);

        LocalBroadcastManager.getInstance(ContextUtils.getApplicationContext())
                .unregisterReceiver(receiver);

        verify(receiver).onReceive(any(Context.class), any(Intent.class));
    }

    @Test
    public void testEnableTouchInputBeforeStartedSendsEnableTouchToActivity() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);
        component.enableTouchInput(true);

        component.start(mStartParams);

        Intent intent = mShadowApplication.getNextStartedActivity();

        Assert.assertTrue(CastWebContentsIntentUtils.isTouchable(intent));
    }

    @Test
    public void testDisableTouchInputBeforeStartedSendsEnableTouchToActivity() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);
        component.enableTouchInput(false);

        component.start(mStartParams);

        Intent intent = mShadowApplication.getNextStartedActivity();

        Assert.assertFalse(CastWebContentsIntentUtils.isTouchable(intent));
    }

    @Test
    public void testOnComponentClosedCallsCallback() {
        CastWebContentsComponent.OnComponentClosedHandler callback =
                Mockito.mock(CastWebContentsComponent.OnComponentClosedHandler.class);

        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, callback, null, false, true, false);
        component.start(mStartParams);
        CastWebContentsComponent.onComponentClosed(SESSION_ID);
        verify(callback).onComponentClosed();

        component.stop();
    }

    @Test
    public void testStopDoesNotUnbindServiceIfStartWasNotCalled() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);

        component.stop();

        Assert.assertNull(mShadowApplication.getNextStoppedService());
    }

    @Test
    public void testOnVisibilityChangeCallback() {
        CastWebContentsComponent.SurfaceEventHandler callback =
                Mockito.mock(CastWebContentsComponent.SurfaceEventHandler.class);

        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, callback, false, true, false);
        component.start(mStartParams);
        CastWebContentsComponent.onVisibilityChange(SESSION_ID, 2);
        component.stop();

        verify(callback).onVisibilityChange(2);
    }

    @Test
    public void testStartWebContentsComponentMultipleTimes() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);
        component.start(mStartParams);
        Assert.assertTrue(component.isStarted());
        var activity = mShadowApplication.getNextStartedActivity();
        Assert.assertNotNull(activity);
        Assert.assertEquals(
                activity.getComponent().getClassName(), CastWebContentsActivity.class.getName());

        StartParams params2 = new StartParams(mWebContents, "test", true);
        component.start(params2);
        Assert.assertTrue(component.isStarted());
        activity = mShadowApplication.getNextStartedActivity();
        Assert.assertNotNull(activity);
        Assert.assertEquals(
                activity.getComponent().getClassName(), CastWebContentsActivity.class.getName());

        component.stop();
        Assert.assertFalse(component.isStarted());
    }

    @Test
    public void testSetMediaPlayingBroadcastsMediaStatus() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);
        Intent receivedIntent0 =
                verifyBroadcastedIntent(
                        new IntentFilter(CastWebContentsIntentUtils.ACTION_MEDIA_PLAYING),
                        () -> component.setMediaPlaying(true),
                        true);
        Assert.assertTrue(CastWebContentsIntentUtils.isMediaPlaying(receivedIntent0));
        Intent receivedIntent1 =
                verifyBroadcastedIntent(
                        new IntentFilter(CastWebContentsIntentUtils.ACTION_MEDIA_PLAYING),
                        () -> component.setMediaPlaying(false),
                        true);
        Assert.assertFalse(CastWebContentsIntentUtils.isMediaPlaying(receivedIntent1));
    }

    @Test
    public void testRequestMediaStatusBroadcastsMediaStatus() {
        String sessionId = "abcdef0";
        CastWebContentsComponent component =
                new CastWebContentsComponent(sessionId, null, null, false, true, false);
        component.start(mStartParams);
        Assert.assertTrue(component.isStarted());
        component.setMediaPlaying(false);
        Intent receivedIntent0 =
                verifyBroadcastedIntent(
                        new IntentFilter(CastWebContentsIntentUtils.ACTION_MEDIA_PLAYING),
                        () -> requestMediaPlayingStatus(sessionId),
                        true);
        Assert.assertFalse(CastWebContentsIntentUtils.isMediaPlaying(receivedIntent0));
        component.setMediaPlaying(true);
        Intent receivedIntent1 =
                verifyBroadcastedIntent(
                        new IntentFilter(CastWebContentsIntentUtils.ACTION_MEDIA_PLAYING),
                        () -> requestMediaPlayingStatus(sessionId),
                        true);
        Assert.assertTrue(CastWebContentsIntentUtils.isMediaPlaying(receivedIntent1));
    }

    @Test
    public void requestsAudioFocusIfStartParamsAsks() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, true);
        CastWebContentsComponent.StartParams startParams =
                new StartParams(mWebContents, APP_ID, true /* shouldRequestAudioFocus */);
        component.start(startParams);
        Intent intent = mShadowApplication.getNextStartedActivity();
        Assert.assertTrue(CastWebContentsIntentUtils.shouldRequestAudioFocus(intent));
    }

    @Test
    public void doesNotRequestAudioFocusIfStartParamsDoNotAsk() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, true);
        CastWebContentsComponent.StartParams startParams =
                new StartParams(mWebContents, APP_ID, false /* shouldRequestAudioFocus */);
        component.start(startParams);
        Intent intent = mShadowApplication.getNextStartedActivity();
        Assert.assertFalse(CastWebContentsIntentUtils.shouldRequestAudioFocus(intent));
    }

    private void requestMediaPlayingStatus(String sessionId) {
        Intent intent = CastWebContentsIntentUtils.requestMediaPlayingStatus(sessionId);
        LocalBroadcastManager.getInstance(ApplicationProvider.getApplicationContext())
                .sendBroadcastSync(intent);
    }

    private Intent verifyBroadcastedIntent(
            IntentFilter filter, Runnable runnable, boolean shouldExpect) {
        BroadcastReceiver receiver = mock(BroadcastReceiver.class);
        LocalBroadcastManager.getInstance(ApplicationProvider.getApplicationContext())
                .registerReceiver(receiver, filter);
        try {
            runnable.run();
        } finally {
            LocalBroadcastManager.getInstance(ApplicationProvider.getApplicationContext())
                    .unregisterReceiver(receiver);
        }
        if (shouldExpect) {
            verify(receiver).onReceive(any(Context.class), mIntentCaptor.capture());
        } else {
            verify(receiver, times(0)).onReceive(any(Context.class), mIntentCaptor.getValue());
        }
        return mIntentCaptor.getValue();
    }
}
