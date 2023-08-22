// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static android.os.Looper.getMainLooper;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.PictureInPictureParams;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
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
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowActivity;
import org.robolectric.shadows.ShadowActivityManager;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.chromecast.base.Observer;
import org.chromium.chromecast.base.Scope;
import org.chromium.chromecast.base.Unit;
import org.chromium.content_public.browser.WebContents;

/**
 * Tests for CastWebContentsActivity.
 *
 * TODO(sanfin): Add more tests.
 */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(Mode.PAUSED)
public class CastWebContentsActivityTest {
    /**
     * ShadowActivity that allows us to intercept calls to setTurnScreenOn.
     */
    @Implements(Activity.class)
    public static class ExtendedShadowActivity extends ShadowActivity {
        private boolean mTurnScreenOn;
        private boolean mShowWhenLocked;
        private boolean mInPipMode;
        private MotionEvent mLastTouchEvent;

        @Override
        public boolean getTurnScreenOn() {
            return mTurnScreenOn;
        }

        @Override
        public boolean getShowWhenLocked() {
            return mShowWhenLocked;
        }

        public boolean getInPictureInPictureMode() {
            return mInPipMode;
        }

        public MotionEvent popLastTouchEvent() {
            MotionEvent result = mLastTouchEvent;
            mLastTouchEvent = null;
            return result;
        }

        @Implementation
        @Override
        public void setTurnScreenOn(boolean turnScreenOn) {
            mTurnScreenOn = turnScreenOn;
        }

        @Implementation
        @Override
        public void setShowWhenLocked(boolean showWhenLocked) {
            mShowWhenLocked = showWhenLocked;
        }

        @Implementation
        public boolean dispatchTouchEvent(MotionEvent ev) {
            mLastTouchEvent = ev;
            return true;
        }

        @Override
        public boolean enterPictureInPictureMode(PictureInPictureParams params) {
            mInPipMode = true;
            return true;
        }
    }

    private ShadowActivityManager mShadowActivityManager;
    private ShadowPackageManager mShadowPackageManager;
    private ActivityController<CastWebContentsActivity> mActivityLifecycle;
    private CastWebContentsActivity mActivity;
    private ShadowActivity mShadowActivity;
    private @Mock WebContents mWebContents;
    private String mSessionId;

    @Captor
    private ArgumentCaptor<Intent> mIntentCaptor;

    private static Intent defaultIntentForCastWebContentsActivity(WebContents webContents) {
        return CastWebContentsIntentUtils.requestStartCastActivity(
                RuntimeEnvironment.application, webContents, true, false, true, false, "0");
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Intent defaultIntent = defaultIntentForCastWebContentsActivity(mWebContents);
        mSessionId = CastWebContentsIntentUtils.getSessionId(defaultIntent.getExtras());
        mShadowActivityManager =
                Shadows.shadowOf((ActivityManager) RuntimeEnvironment.application.getSystemService(
                        Context.ACTIVITY_SERVICE));
        mShadowPackageManager =
                Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager());
        mActivityLifecycle =
                Robolectric.buildActivity(CastWebContentsActivity.class, defaultIntent);
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        mShadowActivity = Shadows.shadowOf(mActivity);

        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.application);
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
                RuntimeEnvironment.application, newWebContents, true, false, true, false, null);
        intent.removeExtra(CastWebContentsIntentUtils.INTENT_EXTRA_URI);
        mActivity.setSurfaceHelperForTesting(surfaceHelper);
        mActivityLifecycle.create();
        reset(surfaceHelper);
        mActivityLifecycle.newIntent(intent);
        verify(surfaceHelper, never()).onNewStartParams(any());
    }

    @Test
    public void testDropsIntentWithoutWebContents() {
        CastWebContentsSurfaceHelper surfaceHelper = mock(CastWebContentsSurfaceHelper.class);
        Intent intent = CastWebContentsIntentUtils.requestStartCastActivity(
                RuntimeEnvironment.application, null, true, false, true, false, "1");
        mActivity.setSurfaceHelperForTesting(surfaceHelper);
        mActivityLifecycle.create();
        reset(surfaceHelper);
        mActivityLifecycle.newIntent(intent);
        verify(surfaceHelper, never()).onNewStartParams(any());
    }

    @Test
    public void testNotifiesSurfaceHelperWithValidIntent() {
        CastWebContentsSurfaceHelper surfaceHelper = mock(CastWebContentsSurfaceHelper.class);
        WebContents newWebContents = mock(WebContents.class);
        Intent intent = CastWebContentsIntentUtils.requestStartCastActivity(
                RuntimeEnvironment.application, newWebContents, true, false, true, false, "2");
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
        verify(surfaceHelper, never()).onNewStartParams(any());
    }

    @Test
    @Config(shadows = {ExtendedShadowActivity.class})
    public void testTurnsScreenOnIfTurnOnScreen() {
        mActivityLifecycle = Robolectric.buildActivity(CastWebContentsActivity.class,
                CastWebContentsIntentUtils.requestStartCastActivity(RuntimeEnvironment.application,
                        mWebContents, true, false, true, false, "0"));
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
                CastWebContentsIntentUtils.requestStartCastActivity(RuntimeEnvironment.application,
                        mWebContents, true, false, true, false, "0"));
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
                CastWebContentsIntentUtils.requestStartCastActivity(RuntimeEnvironment.application,
                        mWebContents, true, false, false, false, "0"));
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
                CastWebContentsIntentUtils.requestStartCastActivity(RuntimeEnvironment.application,
                        mWebContents, true, false, true, false, "0"));
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        mActivityLifecycle.create();

        Assert.assertTrue(Shadows.shadowOf(mActivity.getWindow())
                                  .getFlag(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON));
    }

    @Test
    public void testKeepsScreenOnIfRequested() {
        mActivityLifecycle = Robolectric.buildActivity(CastWebContentsActivity.class,
                CastWebContentsIntentUtils.requestStartCastActivity(RuntimeEnvironment.application,
                        mWebContents, true, false, true, true, "0"));
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        mActivityLifecycle.create();

        Assert.assertTrue(Shadows.shadowOf(mActivity.getWindow())
                                  .getFlag(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON));
    }

    @Test
    public void testDoesNotKeepScreenOnIfNotRequested() {
        mActivityLifecycle = Robolectric.buildActivity(CastWebContentsActivity.class,
                CastWebContentsIntentUtils.requestStartCastActivity(RuntimeEnvironment.application,
                        mWebContents, true, false, true, false, "0"));
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        mActivityLifecycle.create();

        Assert.assertFalse(Shadows.shadowOf(mActivity.getWindow())
                                   .getFlag(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON));
    }

    // TODO(guohuideng): Add unit test for PiP when the Robolectric in internal codebase is
    // ready.

    @Test
    public void testStopDoesNotCauseFinish() {
        mActivityLifecycle.create().start().resume();
        mActivityLifecycle.pause().stop();
        Assert.assertFalse(mActivity.isFinishing());
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
        Assert.assertFalse(mActivity.isFinishing());
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
    @Config(shadows = {ExtendedShadowActivity.class}, sdk = {Build.VERSION_CODES.O})
    public void testStopWhileNotInPipModeDoesNotCloseActivity() {
        mShadowActivityManager.setLockTaskModeState(ActivityManager.LOCK_TASK_MODE_NONE);
        ExtendedShadowActivity shadowActivity = (ExtendedShadowActivity) Shadow.extract(mActivity);
        mActivityLifecycle.create().start().resume();
        verifyBroadcastedIntent(
                filterFor(CastWebContentsIntentUtils.ACTION_ACTIVITY_STOPPED), () -> {
                    mActivityLifecycle.pause().stop();
                    assertFalse(mActivity.isFinishing());
                }, false);
    }

    @Test
    @Config(shadows = {ExtendedShadowActivity.class}, sdk = {Build.VERSION_CODES.O})
    public void testStopWhileInPipModeDoesNotClosesActivity() {
        mShadowActivityManager.setLockTaskModeState(ActivityManager.LOCK_TASK_MODE_NONE);
        ExtendedShadowActivity shadowActivity = (ExtendedShadowActivity) Shadow.extract(mActivity);
        mActivityLifecycle.create().start().resume();
        mActivity.onUserLeaveHint();
        mActivity.onPictureInPictureModeChanged(true, null);
        verifyBroadcastedIntent(
                filterFor(CastWebContentsIntentUtils.ACTION_ACTIVITY_STOPPED), () -> {
                    mActivityLifecycle.pause().stop();
                    assertFalse(mActivity.isFinishing());
                }, false);
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
                    assertFalse(mActivity.isFinishing());
                }, false);
    }

    @Test
    public void testClosesWhenActivityStopsInLockTaskMode() {
        mShadowActivityManager.setLockTaskModeState(ActivityManager.LOCK_TASK_MODE_LOCKED);
        mActivityLifecycle.create().start().resume();
        verifyBroadcastedIntent(
                filterFor(CastWebContentsIntentUtils.ACTION_ACTIVITY_STOPPED), () -> {
                    mActivityLifecycle.pause().stop();
                    assertTrue(mActivity.isFinishing());
                }, true);
    }

    @Test
    public void testClosesWhenActivityStopsInLockTaskModePinned() {
        mShadowActivityManager.setLockTaskModeState(ActivityManager.LOCK_TASK_MODE_PINNED);
        mActivityLifecycle.create().start().resume();
        verifyBroadcastedIntent(
                filterFor(CastWebContentsIntentUtils.ACTION_ACTIVITY_STOPPED), () -> {
                    mActivityLifecycle.pause().stop();
                    assertTrue(mActivity.isFinishing());
                }, true);
    }

    @Test
    public void testKeepsScreenOnInLockTaskMode() {
        mShadowActivityManager.setLockTaskModeState(ActivityManager.LOCK_TASK_MODE_PINNED);
        mActivityLifecycle.create().start().resume();

        Assert.assertTrue(Shadows.shadowOf(mActivityLifecycle.get().getWindow())
                                  .getFlag(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON));
    }

    @Test
    @Config(shadows = {ExtendedShadowActivity.class})
    public void testEntersPipWhenAllowPipIsTrue() {
        mShadowPackageManager.setSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE, true);
        mActivityLifecycle.create().start().resume();

        CastWebContentsIntentUtils.getLocalBroadcastManager().sendBroadcastSync(
                CastWebContentsIntentUtils.allowPictureInPicture(mSessionId, true));
        mActivity.onUserLeaveHint();

        ExtendedShadowActivity shadowActivity = (ExtendedShadowActivity) Shadow.extract(mActivity);
        assertTrue(shadowActivity.getInPictureInPictureMode());
    }

    @Test
    @Config(shadows = {ExtendedShadowActivity.class})
    public void testDoesNotenterPipWhenAllowPipIsFalse() {
        mShadowPackageManager.setSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE, true);
        mActivityLifecycle.create().start().resume();

        CastWebContentsIntentUtils.getLocalBroadcastManager().sendBroadcastSync(
                CastWebContentsIntentUtils.allowPictureInPicture(mSessionId, false));
        mActivity.onUserLeaveHint();

        ExtendedShadowActivity shadowActivity = (ExtendedShadowActivity) Shadow.extract(mActivity);
        assertFalse(shadowActivity.getInPictureInPictureMode());
    }

    @Test
    @Config(shadows = {ExtendedShadowActivity.class})
    public void testEntersPipWhenAllowPipIsTrueOnUserPresent() {
        mShadowPackageManager.setSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE, true);
        mActivityLifecycle.create().start().resume();

        CastWebContentsIntentUtils.getLocalBroadcastManager().sendBroadcastSync(
                CastWebContentsIntentUtils.allowPictureInPicture(mSessionId, true));
        RuntimeEnvironment.application.sendBroadcast(new Intent(Intent.ACTION_USER_PRESENT));

        ExtendedShadowActivity shadowActivity = (ExtendedShadowActivity) Shadow.extract(mActivity);
        Shadows.shadowOf(getMainLooper()).idle();

        assertTrue(shadowActivity.getInPictureInPictureMode());
    }

    @Test
    @Config(shadows = {ExtendedShadowActivity.class})
    public void testDoesNotenterPipWhenAllowPipIsFalseOnUserPresent() {
        mShadowPackageManager.setSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE, true);
        mActivityLifecycle.create().start().resume();

        CastWebContentsIntentUtils.getLocalBroadcastManager().sendBroadcastSync(
                CastWebContentsIntentUtils.allowPictureInPicture(mSessionId, false));
        RuntimeEnvironment.application.sendBroadcast(new Intent(Intent.ACTION_USER_PRESENT));

        ExtendedShadowActivity shadowActivity = (ExtendedShadowActivity) Shadow.extract(mActivity);
        Shadows.shadowOf(getMainLooper()).idle();

        assertFalse(shadowActivity.getInPictureInPictureMode());
    }

    @Test
    public void testSurfaceAvailable() {
        Observer<Unit> observer = mock(Observer.class);
        Scope scope = mock(Scope.class);
        when(observer.open(any())).thenReturn(scope);

        mActivity.mSurfaceAvailable.subscribe(observer);

        mActivityLifecycle.create().start().resume();
        verify(observer).open(any());

        mActivity.onUserLeaveHint();
        verify(scope).close();
    }

    @Test
    public void testAddsRequiredFlagsForDifferentDockedAndMediaPlayingStateTransistions() {
        mActivityLifecycle = Robolectric.buildActivity(CastWebContentsActivity.class,
                CastWebContentsIntentUtils.requestStartCastActivity(RuntimeEnvironment.application,
                        mWebContents, true, false, true, /*keepScreenOn=*/false, "0"));
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        mActivityLifecycle.create();
        // RuntimeEnvironment.application
        updateDockState(false);
        updateMediaState(false);
        // State: Undocked & No Media Playing
        assertWakeLockFlags(false, false);
        // Media Starts playing
        updateMediaState(true);
        // State: Undocked & Media Playing
        assertWakeLockFlags(false, false);
        // Device docked
        updateDockState(true);
        // State: Docked & Media Playing
        assertWakeLockFlags(true, true);
        // Media Stops playing
        updateMediaState(false);
        // // State: Docked & No Media Playing
        assertWakeLockFlags(false, true);
        // Media Starts playing again
        updateMediaState(true);
        // State: Docked & Media Playing
        assertWakeLockFlags(true, true);
        // Undocks
        updateDockState(false);
        // State: Undocked & Media Playing
        assertWakeLockFlags(false, false);
        updateMediaState(false);
        // State: Undocked & No Media Playing
        assertWakeLockFlags(false, false);
    }

    @Test
    public void testEnsureDockStateAndMediaStateDoNotImpactKeepScreenOnFlagIfAlwaysKeepScreenOn() {
        mActivityLifecycle = Robolectric.buildActivity(CastWebContentsActivity.class,
                CastWebContentsIntentUtils.requestStartCastActivity(RuntimeEnvironment.application,
                        mWebContents, true, false, true, /*keepScreenOn=*/true, "0"));
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        mActivityLifecycle.create();
        updateDockState(false);
        updateMediaState(false);
        assertWakeLockFlags(true, false);
        updateDockState(true);
        updateMediaState(true);
        assertWakeLockFlags(true, true);
        updateDockState(false);
        updateMediaState(false);
        assertWakeLockFlags(false, false);
    }

    @Test
    public void testEnsureBroadcastMediaStatusRequestedOnCreation() {
        updateDockState(true);
        BroadcastReceiver receiver = spy(new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (CastWebContentsIntentUtils.isIntentOfRequestMediaPlayingStatus(intent)) {
                    updateMediaState(true);
                }
            }
        });

        IntentFilter filter = new IntentFilter();
        Uri instanceUri = CastWebContentsIntentUtils.getInstanceUri(mSessionId);
        filter.addDataScheme(instanceUri.getScheme());
        filter.addDataAuthority(instanceUri.getAuthority(), null);
        filter.addDataPath(instanceUri.getPath(), PatternMatcher.PATTERN_LITERAL);
        filter.addAction(CastWebContentsIntentUtils.ACTION_REQUEST_MEDIA_PLAYING_STATUS);
        LocalBroadcastManager.getInstance(RuntimeEnvironment.application)
                .registerReceiver(receiver, filter);
        mActivityLifecycle = Robolectric.buildActivity(CastWebContentsActivity.class,
                CastWebContentsIntentUtils.requestStartCastActivity(RuntimeEnvironment.application,
                        mWebContents, true, false, true, /*keepScreenOn=*/false, mSessionId));
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        mActivityLifecycle.create();
        Shadows.shadowOf(getMainLooper()).idle();
        verify(receiver, times(1)).onReceive(any(Context.class), mIntentCaptor.capture());
        Intent broadcastIntent = mIntentCaptor.getValue();
        assertEquals(CastWebContentsIntentUtils.ACTION_REQUEST_MEDIA_PLAYING_STATUS,
                broadcastIntent.getAction());
        assertWakeLockFlags(true, true);
    }

    @Test
    public void testTaskRemovedMonitorServiceStartedOnCreation() {
        mActivityLifecycle = Robolectric.buildActivity(CastWebContentsActivity.class,
                CastWebContentsIntentUtils.requestStartCastActivity(RuntimeEnvironment.application,
                        mWebContents, true, false, true, false, mSessionId));
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        mActivityLifecycle.create();
        //  RuntimeEnvironment.application
        Intent serviceIntent =
                Shadows.shadowOf(RuntimeEnvironment.application).getNextStartedService();
        assertNotNull(serviceIntent);
        assertEquals(TaskRemovedMonitorService.class.getName(),
                serviceIntent.getComponent().getClassName());
        assertEquals(mSessionId,
                serviceIntent.getStringExtra(TaskRemovedMonitorService.ROOT_SESSION_KEY));
        assertEquals(
                mSessionId, serviceIntent.getStringExtra(TaskRemovedMonitorService.SESSION_KEY));
    }

    @Test
    public void testTaskRemovedMonitorServiceUpdatedWithNewIntent() {
        mActivityLifecycle = Robolectric.buildActivity(CastWebContentsActivity.class,
                CastWebContentsIntentUtils.requestStartCastActivity(RuntimeEnvironment.application,
                        mWebContents, true, false, true, false, mSessionId));
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        mActivityLifecycle.create();
        //  RuntimeEnvironment.application
        Intent serviceIntent =
                Shadows.shadowOf(RuntimeEnvironment.application).getNextStartedService();
        assertNotNull(serviceIntent);
        assertEquals(TaskRemovedMonitorService.class.getName(),
                serviceIntent.getComponent().getClassName());
        assertEquals(mSessionId,
                serviceIntent.getStringExtra(TaskRemovedMonitorService.ROOT_SESSION_KEY));
        assertEquals(
                mSessionId, serviceIntent.getStringExtra(TaskRemovedMonitorService.SESSION_KEY));
        String newSessionId = "1234-5678-910A";
        Intent newIntent =
                CastWebContentsIntentUtils.requestStartCastActivity(RuntimeEnvironment.application,
                        mWebContents, true, false, true, false, newSessionId);
        mActivityLifecycle.newIntent(newIntent);
        serviceIntent = Shadows.shadowOf(RuntimeEnvironment.application).getNextStartedService();
        assertNotNull(serviceIntent);
        assertEquals(TaskRemovedMonitorService.class.getName(),
                serviceIntent.getComponent().getClassName());
        assertEquals(mSessionId,
                serviceIntent.getStringExtra(TaskRemovedMonitorService.ROOT_SESSION_KEY));
        assertEquals(
                newSessionId, serviceIntent.getStringExtra(TaskRemovedMonitorService.SESSION_KEY));
    }

    @Test
    public void testTaskRemovedMonitorServiceStoppedWhenActivityFinished() {
        mActivityLifecycle.create();
        mActivity.finishForTesting();
        Intent serviceIntent =
                Shadows.shadowOf(RuntimeEnvironment.application).getNextStoppedService();
        assertNotNull(serviceIntent);
        assertEquals(TaskRemovedMonitorService.class.getName(),
                serviceIntent.getComponent().getClassName());
    }

    private void assertWakeLockFlags(boolean keepScreenOn, boolean allowLockWhileScreenOn) {
        if (keepScreenOn) {
            Assert.assertTrue(Shadows.shadowOf(mActivity.getWindow())
                                      .getFlag(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON));
        } else {
            Assert.assertFalse(Shadows.shadowOf(mActivity.getWindow())
                                       .getFlag(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON));
        }
        if (allowLockWhileScreenOn) {
            Assert.assertTrue(
                    Shadows.shadowOf(mActivity.getWindow())
                            .getFlag(WindowManager.LayoutParams.FLAG_ALLOW_LOCK_WHILE_SCREEN_ON));
        } else {
            Assert.assertFalse(
                    Shadows.shadowOf(mActivity.getWindow())
                            .getFlag(WindowManager.LayoutParams.FLAG_ALLOW_LOCK_WHILE_SCREEN_ON));
        }
    }

    private void updateDockState(boolean docked) {
        Intent intent = new Intent(Intent.ACTION_DOCK_EVENT);
        intent.putExtra(
                Intent.EXTRA_DOCK_STATE, Intent.EXTRA_DOCK_STATE_UNDOCKED + (docked ? 1 : 0));
        RuntimeEnvironment.application.sendStickyBroadcast(intent);
        Shadows.shadowOf(getMainLooper()).idle();
    }

    private void updateMediaState(boolean playingMedia) {
        CastWebContentsIntentUtils.getLocalBroadcastManager().sendBroadcastSync(
                CastWebContentsIntentUtils.mediaPlaying(mSessionId, playingMedia));
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
