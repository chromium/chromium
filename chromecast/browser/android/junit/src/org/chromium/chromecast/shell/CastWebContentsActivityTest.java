// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyObject;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Build;
import android.os.PatternMatcher;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Window;
import android.view.WindowManager;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowActivity;
import org.robolectric.shadows.ShadowActivityManager;

import org.chromium.content_public.browser.WebContents;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests for CastWebContentsActivity.
 *
 * TODO(sanfin): Add more tests.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CastWebContentsActivityTest {
    /**
     * ShadowActivity that allows us to intercept calls to setTurnScreenOn.
     */
    @Implements(Activity.class)
    public static class ExtendedShadowActivity extends ShadowActivity {
        private boolean mTurnScreenOn;
        private boolean mShowWhenLocked;
        private MotionEvent mLastTouchEvent;

        public boolean getTurnScreenOn() {
            return mTurnScreenOn;
        }

        public boolean getShowWhenLocked() {
            return mShowWhenLocked;
        }

        public MotionEvent popLastTouchEvent() {
            MotionEvent result = mLastTouchEvent;
            mLastTouchEvent = null;
            return result;
        }

        @Implementation
        public void setTurnScreenOn(boolean turnScreenOn) {
            mTurnScreenOn = turnScreenOn;
        }

        @Implementation
        public void setShowWhenLocked(boolean showWhenLocked) {
            mShowWhenLocked = showWhenLocked;
        }

        @Implementation
        public boolean dispatchTouchEvent(MotionEvent ev) {
            mLastTouchEvent = ev;
            return true;
        }
    }

    private ShadowActivityManager mShadowActivityManager;
    private ActivityController<CastWebContentsActivity> mActivityLifecycle;
    private CastWebContentsActivity mActivity;
    private ShadowActivity mShadowActivity;
    private @Mock WebContents mWebContents;
    private String mSessionId;

    private static Intent defaultIntentForCastWebContentsActivity(WebContents webContents) {
        return CastWebContentsIntentUtils.requestStartCastActivity(
                RuntimeEnvironment.application, webContents, true, false, true, "0");
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Intent defaultIntent = defaultIntentForCastWebContentsActivity(mWebContents);
        mSessionId = CastWebContentsIntentUtils.getSessionId(defaultIntent.getExtras());
        mShadowActivityManager =
                Shadows.shadowOf((ActivityManager) RuntimeEnvironment.application.getSystemService(
                        Context.ACTIVITY_SERVICE));
        mActivityLifecycle =
                Robolectric.buildActivity(CastWebContentsActivity.class, defaultIntent);
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        mShadowActivity = Shadows.shadowOf(mActivity);
    }

    @Test
    public void testSetsVolumeControlStream() {
        mActivityLifecycle.create();

        assertEquals(AudioManager.STREAM_MUSIC, mActivity.getVolumeControlStream());
    }

    @Test
    public void testNewIntentAfterFinishLaunchesNewActivity() {
        mActivityLifecycle.create();
        mActivity.finishForTesting();
        Intent intent = new Intent(Intent.ACTION_VIEW, null, RuntimeEnvironment.application,
                CastWebContentsActivity.class);
        mActivityLifecycle.newIntent(intent);
        Intent next = mShadowActivity.getNextStartedActivity();
        assertEquals(next.getComponent().getClassName(), CastWebContentsActivity.class.getName());
    }

    @Test
    public void testFinishDoesNotLaunchNewActivity() {
        mActivityLifecycle.create();
        mActivity.finishForTesting();
        Intent intent = mShadowActivity.getNextStartedActivity();
        assertNull(intent);
    }

    @Test
    public void testDropsIntentWithoutUri() {
        CastWebContentsSurfaceHelper surfaceHelper = mock(CastWebContentsSurfaceHelper.class);
        WebContents newWebContents = mock(WebContents.class);
        Intent intent = CastWebContentsIntentUtils.requestStartCastActivity(
                RuntimeEnvironment.application, newWebContents, true, false, true, null);
        intent.removeExtra(CastWebContentsIntentUtils.INTENT_EXTRA_URI);
        mActivity.setSurfaceHelperForTesting(surfaceHelper);
        mActivityLifecycle.create();
        reset(surfaceHelper);
        mActivityLifecycle.newIntent(intent);
        verify(surfaceHelper, never()).onNewStartParams(anyObject());
    }

    @Test
    public void testDropsIntentWithoutWebContents() {
        CastWebContentsSurfaceHelper surfaceHelper = mock(CastWebContentsSurfaceHelper.class);
        Intent intent = CastWebContentsIntentUtils.requestStartCastActivity(
                RuntimeEnvironment.application, null, true, false, true, "1");
        mActivity.setSurfaceHelperForTesting(surfaceHelper);
        mActivityLifecycle.create();
        reset(surfaceHelper);
        mActivityLifecycle.newIntent(intent);
        verify(surfaceHelper, never()).onNewStartParams(anyObject());
    }

    @Test
    public void testNotifiesSurfaceHelperWithValidIntent() {
        CastWebContentsSurfaceHelper surfaceHelper = mock(CastWebContentsSurfaceHelper.class);
        WebContents newWebContents = mock(WebContents.class);
        Intent intent = CastWebContentsIntentUtils.requestStartCastActivity(
                RuntimeEnvironment.application, newWebContents, true, false, true, "2");
        mActivity.setSurfaceHelperForTesting(surfaceHelper);
        mActivityLifecycle.create();
        reset(surfaceHelper);
        mActivityLifecycle.newIntent(intent);
        verify(surfaceHelper)
                .onNewStartParams(new CastWebContentsSurfaceHelper.StartParams(
                        CastWebContentsIntentUtils.getInstanceUri("2"), newWebContents, false,
                        true));
    }

    @Test
    public void testDropsIntentWithDuplicateUri() {
        CastWebContentsSurfaceHelper surfaceHelper = mock(CastWebContentsSurfaceHelper.class);
        mActivity.setSurfaceHelperForTesting(surfaceHelper);
        mActivityLifecycle.create();
        reset(surfaceHelper);
        // Send duplicate Intent.
        Intent intent = defaultIntentForCastWebContentsActivity(mWebContents);
        mActivityLifecycle.newIntent(intent);
        verify(surfaceHelper, never()).onNewStartParams(anyObject());
    }

    @Test
    @Config(shadows = {ExtendedShadowActivity.class})
    public void testTurnsScreenOnIfTurnOnScreen() {
        mActivityLifecycle = Robolectric.buildActivity(CastWebContentsActivity.class,
                CastWebContentsIntentUtils.requestStartCastActivity(
                        RuntimeEnvironment.application, mWebContents, true, false, true, "0"));
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        ExtendedShadowActivity shadowActivity = (ExtendedShadowActivity) Shadow.extract(mActivity);
        mActivityLifecycle.create();

        Assert.assertTrue(shadowActivity.getTurnScreenOn());
        Assert.assertTrue(shadowActivity.getShowWhenLocked());
    }

    @Test
    @Config(sdk = {Build.VERSION_CODES.O})
    public void testTurnsScreenOnIfTurnOnScreen_AndroidO() {
        mActivityLifecycle = Robolectric.buildActivity(CastWebContentsActivity.class,
                CastWebContentsIntentUtils.requestStartCastActivity(
                        RuntimeEnvironment.application, mWebContents, true, false, true, "0"));
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        mActivityLifecycle.create();

        Assert.assertTrue(Shadows.shadowOf(mActivity.getWindow())
                                  .getFlag(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON));
    }

    @Test
    @Config(shadows = {ExtendedShadowActivity.class})
    public void testDoesNotTurnScreenOnIfNotTurnOnScreen() {
        mActivityLifecycle = Robolectric.buildActivity(CastWebContentsActivity.class,
                CastWebContentsIntentUtils.requestStartCastActivity(
                        RuntimeEnvironment.application, mWebContents, true, false, false, "0"));
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        ExtendedShadowActivity shadowActivity = (ExtendedShadowActivity) Shadow.extract(mActivity);
        mActivityLifecycle.create();

        Assert.assertFalse(shadowActivity.getTurnScreenOn());
        Assert.assertFalse(shadowActivity.getShowWhenLocked());
    }

    @Test
    @Config(sdk = {Build.VERSION_CODES.O})
    public void testDoesNotTurnScreenOnIfNotTurnOnScreen_AndroidO() {
        mActivityLifecycle = Robolectric.buildActivity(CastWebContentsActivity.class,
                CastWebContentsIntentUtils.requestStartCastActivity(
                        RuntimeEnvironment.application, mWebContents, true, false, true, "0"));
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        mActivityLifecycle.create();

        Assert.assertTrue(Shadows.shadowOf(mActivity.getWindow())
                                  .getFlag(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON));
    }

    // TODO(guohuideng): Add unit test for PiP when the Robolectric in internal codebase is
    // ready.

    @Test
    public void testStopDoesNotCauseFinish() {
        mActivityLifecycle.create().start().resume();
        mActivityLifecycle.pause().stop();
        Assert.assertFalse(mShadowActivity.isFinishing());
    }

    @Test
    public void testOnDestroyDestroysSurfaceHelper() {
        CastWebContentsSurfaceHelper surfaceHelper = mock(CastWebContentsSurfaceHelper.class);
        mActivity.setSurfaceHelperForTesting(surfaceHelper);
        mActivityLifecycle.create().start().resume();
        mActivityLifecycle.pause().stop();
        verify(surfaceHelper, never()).onDestroy();
        mActivityLifecycle.destroy();
        verify(surfaceHelper).onDestroy();
    }

    @Test
    public void testBackButtonDoesNotCauseFinish() {
        mActivityLifecycle.create().start().resume();
        mActivity.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK));
        Assert.assertFalse(mShadowActivity.isFinishing());
    }

    @Test
    public void testDispatchTouchEventWithTouchDisabled() {
        CastWebContentsSurfaceHelper surfaceHelper = mock(CastWebContentsSurfaceHelper.class);
        when(surfaceHelper.isTouchInputEnabled()).thenReturn(false);
        mActivity.setSurfaceHelperForTesting(surfaceHelper);
        mActivityLifecycle.create().start().resume();
        MotionEvent event = mock(MotionEvent.class);
        assertFalse(mActivity.dispatchTouchEvent(event));
    }

    @Test
    public void testDispatchTouchEventWithTouchEnabled() {
        CastWebContentsSurfaceHelper surfaceHelper = mock(CastWebContentsSurfaceHelper.class);
        when(surfaceHelper.isTouchInputEnabled()).thenReturn(true);
        mActivity.setSurfaceHelperForTesting(surfaceHelper);
        Window window = mock(Window.class);
        MotionEvent event = mock(MotionEvent.class);
        when(event.getAction()).thenReturn(MotionEvent.ACTION_DOWN);
        when(window.superDispatchTouchEvent(event)).thenReturn(true);
        mActivityLifecycle.create().start().resume();
        mShadowActivity.setWindow(window);
        assertTrue(mActivity.dispatchTouchEvent(event));
    }

    @Test
    public void testDispatchTouchEventWithTouchEnabledButWindowDoesNotHandleIt() {
        CastWebContentsSurfaceHelper surfaceHelper = mock(CastWebContentsSurfaceHelper.class);
        when(surfaceHelper.isTouchInputEnabled()).thenReturn(true);
        mActivity.setSurfaceHelperForTesting(surfaceHelper);
        Window window = mock(Window.class);
        MotionEvent event = mock(MotionEvent.class);
        when(event.getAction()).thenReturn(MotionEvent.ACTION_DOWN);
        when(window.superDispatchTouchEvent(event)).thenReturn(false);
        mActivityLifecycle.create().start().resume();
        mShadowActivity.setWindow(window);
        assertFalse(mActivity.dispatchTouchEvent(event));
    }

    @Test
    public void testDispatchTouchEventWithNoSurfaceHelper() {
        mActivityLifecycle.create().start().resume();
        MotionEvent event = mock(MotionEvent.class);
        assertFalse(mActivity.dispatchTouchEvent(event));
    }

    @Test
    @Config(shadows = {ExtendedShadowActivity.class})
    public void testDispatchTouchEventInPipMode() {
        CastWebContentsSurfaceHelper surfaceHelper = mock(CastWebContentsSurfaceHelper.class);
        ExtendedShadowActivity shadowActivity = (ExtendedShadowActivity) Shadow.extract(mActivity);
        when(surfaceHelper.isTouchInputEnabled()).thenReturn(true);
        mActivity.setSurfaceHelperForTesting(surfaceHelper);
        Window window = mock(Window.class);
        mActivityLifecycle.create().start().resume();
        shadowActivity.setWindow(window);
        MotionEvent event = mock(MotionEvent.class);
        when(event.getAction()).thenReturn(MotionEvent.ACTION_DOWN);
        when(window.superDispatchTouchEvent(event)).thenReturn(true);
        // Sanity check: touch is enabled before entering PiP mode.
        assertTrue(mActivity.dispatchTouchEvent(event));
        assertEquals(shadowActivity.popLastTouchEvent(), event);
        mActivity.onUserLeaveHint();
        mActivity.onPictureInPictureModeChanged(true, null);
        // Touch is disabled while in PiP mode.
        assertFalse(mActivity.dispatchTouchEvent(event));
        assertNull(shadowActivity.popLastTouchEvent());
        mActivity.onPictureInPictureModeChanged(false, null);
        // Touch is re-enabled after leaving PiP mode.
        assertTrue(mActivity.dispatchTouchEvent(event));
    }

    @Test
    public void
    testComponentNotClosedWhenDestroyedBeforeIsFinishingStateAndActitivityIsNotFinishing() {
        mActivityLifecycle.create();
        verifyBroadcastedIntent(filterFor(CastWebContentsIntentUtils.ACTION_ACTIVITY_STOPPED),
                () -> mActivityLifecycle.destroy(), false);
    }

    @Test
    public void testComponentNotClosedWhenDestroyedAfterIsFinishingStateAndActivityIsFinishing() {
        mActivityLifecycle.create();
        mActivity.finishForTesting();
        verifyBroadcastedIntent(filterFor(CastWebContentsIntentUtils.ACTION_ACTIVITY_STOPPED),
                () -> mActivityLifecycle.destroy(), false);
    }

    @Test
    public void testDoesNotCloseAppWhenActivityStops() {
        mShadowActivityManager.setLockTaskModeState(ActivityManager.LOCK_TASK_MODE_NONE);
        mActivityLifecycle.create().start().resume();
        verifyBroadcastedIntent(
                filterFor(CastWebContentsIntentUtils.ACTION_ACTIVITY_STOPPED), () -> {
                    mActivityLifecycle.pause().stop();
                    assertFalse(mShadowActivity.isFinishing());
                }, false);
    }

    @Test
    public void testClosesWhenActivityStopsInLockTaskMode() {
        mShadowActivityManager.setLockTaskModeState(ActivityManager.LOCK_TASK_MODE_LOCKED);
        mActivityLifecycle.create().start().resume();
        verifyBroadcastedIntent(
                filterFor(CastWebContentsIntentUtils.ACTION_ACTIVITY_STOPPED), () -> {
                    mActivityLifecycle.pause().stop();
                    assertTrue(mShadowActivity.isFinishing());
                }, true);
    }

    @Test
    public void testClosesWhenActivityStopsInLockTaskModePinned() {
        mShadowActivityManager.setLockTaskModeState(ActivityManager.LOCK_TASK_MODE_PINNED);
        mActivityLifecycle.create().start().resume();
        verifyBroadcastedIntent(
                filterFor(CastWebContentsIntentUtils.ACTION_ACTIVITY_STOPPED), () -> {
                    mActivityLifecycle.pause().stop();
                    assertTrue(mShadowActivity.isFinishing());
                }, true);
    }

    private IntentFilter filterFor(String action) {
        IntentFilter filter = new IntentFilter();
        Uri instanceUri = CastWebContentsIntentUtils.getInstanceUri(mSessionId);
        filter.addDataScheme(instanceUri.getScheme());
        filter.addDataAuthority(instanceUri.getAuthority(), null);
        filter.addDataPath(instanceUri.getPath(), PatternMatcher.PATTERN_LITERAL);
        filter.addAction(action);
        return filter;
    }

    private void verifyBroadcastedIntent(
            IntentFilter filter, Runnable runnable, boolean shouldExpect) {
        BroadcastReceiver receiver = mock(BroadcastReceiver.class);
        LocalBroadcastManager.getInstance(RuntimeEnvironment.application)
                .registerReceiver(receiver, filter);
        try {
            runnable.run();
        } finally {
            LocalBroadcastManager.getInstance(RuntimeEnvironment.application)
                    .unregisterReceiver(receiver);
            if (shouldExpect) {
                verify(receiver).onReceive(any(Context.class), any(Intent.class));
            } else {
                verify(receiver, times(0)).onReceive(any(Context.class), any(Intent.class));
            }
        }
    }
}
