// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.view.Display;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chromecast.shell.CastWebContentsComponent.StartParams;
import org.chromium.content_public.browser.WebContents;

/**
 * Tests for CastWebContentsComponent.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CastWebContentsComponentTest {
    private static final String APP_ID = "app";

    private static final String SESSION_ID = "123456789";

    private static final int DISPLAY_ID = 1;
    private static final String ACTIVITY_OPTIONS_DISPLAY_ID = "android.activity.launchDisplayId";

    private @Mock WebContents mWebContents;
    private @Mock Display mDisplay;
    private Activity mActivity;
    private ShadowActivity mShadowActivity;
    private StartParams mStartParams;

    @Captor
    private ArgumentCaptor<Intent> mIntentCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mDisplay.getDisplayId()).thenReturn(DISPLAY_ID);
        mActivity = Mockito.spy(Robolectric.buildActivity(Activity.class).setup().get());
        mShadowActivity = Shadows.shadowOf(mActivity);
        mStartParams = new StartParams(mActivity, mWebContents, APP_ID, false);
    }

    @Test
    public void testStartStartsWebContentsActivity() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);
        component.start(mStartParams, false);
        Intent intent = mShadowActivity.getNextStartedActivity();
        Assert.assertEquals(
                intent.getComponent().getClassName(), CastWebContentsActivity.class.getName());

        component.stop(mActivity);
    }

    @Test
    @Config(minSdk = VERSION_CODES.R)
    public void testStartStartsWebContentsActivityWithDisplayId() {
        ContextWrapper context =
                Mockito.spy(new ContextWrapper(ContextUtils.getApplicationContext()) {
                    @Override
                    public Display getDisplay() {
                        return mDisplay;
                    }
                });
        StartParams startParams = new StartParams(context, mWebContents, APP_ID, false);

        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);
        component.start(startParams, false);

        ArgumentCaptor<Bundle> bundle = ArgumentCaptor.forClass(Bundle.class);
        verify(context).startActivity(any(Intent.class), bundle.capture());
        Assert.assertEquals(bundle.getValue().getInt(ACTIVITY_OPTIONS_DISPLAY_ID), DISPLAY_ID);
    }

    @Test
    public void testStartStartsWebContentsService() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);
        component.start(mStartParams, true);
        component.stop(mActivity);

        ArgumentCaptor<Intent> intent = ArgumentCaptor.forClass(Intent.class);
        verify(mActivity).bindService(
                intent.capture(), any(ServiceConnection.class), eq(Context.BIND_AUTO_CREATE));
        Assert.assertEquals(intent.getValue().getComponent().getClassName(),
                CastWebContentsService.class.getName());
    }

    @Test
    public void testStopSendsStopSignalToActivity() {
        BroadcastReceiver receiver = Mockito.mock(BroadcastReceiver.class);
        IntentFilter intentFilter = new IntentFilter(CastIntents.ACTION_STOP_WEB_CONTENT);
        LocalBroadcastManager.getInstance(ContextUtils.getApplicationContext())
                .registerReceiver(receiver, intentFilter);

        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);
        component.start(mStartParams, false);
        component.stop(ContextUtils.getApplicationContext());

        LocalBroadcastManager.getInstance(ContextUtils.getApplicationContext())
                .unregisterReceiver(receiver);

        verify(receiver).onReceive(any(Context.class), any(Intent.class));
    }

    @Test
    public void testStartBindsWebContentsServiceInHeadlessMode() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);
        component.start(mStartParams, true);
        component.stop(mActivity);

        ArgumentCaptor<Intent> intent = ArgumentCaptor.forClass(Intent.class);
        verify(mActivity).bindService(
                intent.capture(), any(ServiceConnection.class), eq(Context.BIND_AUTO_CREATE));
        Assert.assertEquals(intent.getValue().getComponent().getClassName(),
                CastWebContentsService.class.getName());
    }

    @Test
    public void testStopUnbindsWebContentsServiceInHeadlessMode() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);
        component.start(mStartParams, true);
        component.stop(mActivity);

        verify(mActivity).unbindService(any(ServiceConnection.class));
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

        component.start(mStartParams, false);

        Intent intent = mShadowActivity.getNextStartedActivity();

        Assert.assertTrue(CastWebContentsIntentUtils.isTouchable(intent));
    }

    @Test
    public void testDisableTouchInputBeforeStartedSendsEnableTouchToActivity() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);
        component.enableTouchInput(false);

        component.start(mStartParams, false);

        Intent intent = mShadowActivity.getNextStartedActivity();

        Assert.assertFalse(CastWebContentsIntentUtils.isTouchable(intent));
    }

    @Test
    public void testOnComponentClosedCallsCallback() {
        CastWebContentsComponent.OnComponentClosedHandler callback =
                Mockito.mock(CastWebContentsComponent.OnComponentClosedHandler.class);

        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, callback, null, false, true, false);
        component.start(mStartParams, false);
        CastWebContentsComponent.onComponentClosed(SESSION_ID);
        verify(callback).onComponentClosed();

        component.stop(mActivity);
    }

    @Test
    public void testStopDoesNotUnbindServiceIfStartWasNotCalled() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);

        component.stop(mActivity);

        verify(mActivity, never()).unbindService(any(ServiceConnection.class));
    }

    @Test
    public void testOnVisibilityChangeCallback() {
        CastWebContentsComponent.SurfaceEventHandler callback =
                Mockito.mock(CastWebContentsComponent.SurfaceEventHandler.class);

        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, callback, false, true, false);
        component.start(mStartParams, false);
        CastWebContentsComponent.onVisibilityChange(SESSION_ID, 2);
        component.stop(mActivity);

        verify(callback).onVisibilityChange(2);
    }

    @Test
    public void testStartWebContentsComponentMultipleTimes() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);
        CastWebContentsComponent.Delegate delegate = mock(CastWebContentsComponent.Delegate.class);
        component.start(mStartParams, delegate);
        Assert.assertTrue(component.isStarted());
        verify(delegate, times(1)).start(eq(mStartParams));
        StartParams params2 = new StartParams(mActivity, mWebContents, "test", true);
        component.start(params2, delegate);
        Assert.assertTrue(component.isStarted());
        verify(delegate, times(2)).start(any(StartParams.class));
        verify(delegate, times(1)).start(eq(params2));
        component.stop(mActivity);
        Assert.assertFalse(component.isStarted());
        verify(delegate, times(1)).stop(any(Context.class));
    }

    @Test
    public void testStartActivityDelegateTwiceNoops() {
        // Sending focus events to a started Activity is unnecessary because the Activity is always
        // in focus, and issues with onNewIntent() and duplicate detection can cause unintended
        // side effects.
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);
        CastWebContentsComponent.Delegate delegate = component.new ActivityDelegate();
        component.start(mStartParams, delegate);
        Assert.assertEquals(mShadowActivity.getNextStartedActivity().getComponent().getClassName(),
                CastWebContentsActivity.class.getName());
        component.start(mStartParams, delegate);
        Assert.assertNull(mShadowActivity.getNextStartedActivity());
    }

    @Test
    public void testSetMediaPlayingBroadcastsMediaStatus() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, false);
        Intent receivedIntent0 = verifyBroadcastedIntent(
                new IntentFilter(CastWebContentsIntentUtils.ACTION_MEDIA_PLAYING),
                () -> component.setMediaPlaying(true), true);
        Assert.assertTrue(CastWebContentsIntentUtils.isMediaPlaying(receivedIntent0));
        Intent receivedIntent1 = verifyBroadcastedIntent(
                new IntentFilter(CastWebContentsIntentUtils.ACTION_MEDIA_PLAYING),
                () -> component.setMediaPlaying(false), true);
        Assert.assertFalse(CastWebContentsIntentUtils.isMediaPlaying(receivedIntent1));
    }

    @Test
    public void testRequestMediaStatusBroadcastsMediaStatus() {
        String sessionId = "abcdef0";
        CastWebContentsComponent component =
                new CastWebContentsComponent(sessionId, null, null, false, true, false);
        CastWebContentsComponent.Delegate delegate = mock(CastWebContentsComponent.Delegate.class);
        component.start(mStartParams, delegate);
        Assert.assertTrue(component.isStarted());
        component.setMediaPlaying(false);
        Intent receivedIntent0 = verifyBroadcastedIntent(
                new IntentFilter(CastWebContentsIntentUtils.ACTION_MEDIA_PLAYING),
                () -> requestMediaPlayingStatus(sessionId), true);
        Assert.assertFalse(CastWebContentsIntentUtils.isMediaPlaying(receivedIntent0));
        component.setMediaPlaying(true);
        Intent receivedIntent1 = verifyBroadcastedIntent(
                new IntentFilter(CastWebContentsIntentUtils.ACTION_MEDIA_PLAYING),
                () -> requestMediaPlayingStatus(sessionId), true);
        Assert.assertTrue(CastWebContentsIntentUtils.isMediaPlaying(receivedIntent1));
    }

    @Test
    public void requestsAudioFocusIfStartParamsAsks() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, true);
        CastWebContentsComponent.Delegate delegate = component.new ActivityDelegate();
        CastWebContentsComponent.StartParams startParams = new StartParams(
                mActivity, mWebContents, APP_ID, true /* shouldRequestAudioFocus */);
        component.start(startParams, delegate);
        Intent intent = mShadowActivity.getNextStartedActivity();
        Assert.assertTrue(CastWebContentsIntentUtils.shouldRequestAudioFocus(intent));
    }

    @Test
    public void doesNotRequestAudioFocusIfStartParamsDoNotAsk() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(SESSION_ID, null, null, false, true, true);
        CastWebContentsComponent.Delegate delegate = component.new ActivityDelegate();
        CastWebContentsComponent.StartParams startParams = new StartParams(
                mActivity, mWebContents, APP_ID, false /* shouldRequestAudioFocus */);
        component.start(startParams, delegate);
        Intent intent = mShadowActivity.getNextStartedActivity();
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
            if (shouldExpect) {
                verify(receiver).onReceive(any(Context.class), mIntentCaptor.capture());
            } else {
                verify(receiver, times(0)).onReceive(any(Context.class), mIntentCaptor.getValue());
            }
            return mIntentCaptor.getValue();
        }
    }
}
