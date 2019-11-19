// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.anyObject;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Intent;
import android.media.AudioManager;
import android.os.Build;
import android.view.KeyEvent;
import android.view.WindowManager;

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
    @Implements(Activity.class)
    public static class ExtendedShadowActivity extends ShadowActivity {
        private boolean mTurnScreenOn;

        public boolean getTurnScreenOn() {
            return mTurnScreenOn;
        }

        @Implementation
        public void setTurnScreenOn(boolean turnScreenOn) {
            mTurnScreenOn = turnScreenOn;
        }
    }

    private ActivityController<CastWebContentsActivity> mActivityLifecycle;
    private CastWebContentsActivity mActivity;
    private ShadowActivity mShadowActivity;
    private @Mock WebContents mWebContents;

    private static Intent defaultIntentForCastWebContentsActivity(WebContents webContents) {
        return CastWebContentsIntentUtils.requestStartCastActivity(
                RuntimeEnvironment.application, webContents, true, false, true, "0");
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityLifecycle = Robolectric.buildActivity(CastWebContentsActivity.class,
                defaultIntentForCastWebContentsActivity(mWebContents));
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
    public void testReleasesStreamMuteIfNecessaryOnPause() {
        CastAudioManager mockAudioManager = mock(CastAudioManager.class);
        mActivity.setAudioManagerForTesting(mockAudioManager);
        mActivityLifecycle.create().start().resume();
        mActivityLifecycle.pause();
        verify(mockAudioManager).releaseStreamMuteIfNecessary(AudioManager.STREAM_MUSIC);
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

    @Test
    public void testSetsKeepScreenOnFlag() {
        mActivityLifecycle.create();
        Assert.assertTrue(Shadows.shadowOf(mActivity.getWindow())
                                  .getFlag(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON));
    }

    @Test
    public void testStopDoesNotCauseFinish() {
        mActivityLifecycle.create().start().resume();
        mActivityLifecycle.pause().stop();
        Assert.assertFalse(mShadowActivity.isFinishing());
    }

    @Test
    public void testUserLeaveAndStopCausesFinish() {
        mActivityLifecycle.create().start().resume();
        mActivityLifecycle.pause().userLeaving().stop();
        Assert.assertTrue(mShadowActivity.isFinishing());
    }

    @Test
    public void testUserLeaveAndStopDestroysSurfaceHelper() {
        CastWebContentsSurfaceHelper surfaceHelper = mock(CastWebContentsSurfaceHelper.class);
        mActivity.setSurfaceHelperForTesting(surfaceHelper);
        mActivityLifecycle.create().start().resume();
        mActivityLifecycle.pause().userLeaving().stop();
        verify(surfaceHelper).onDestroy();
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
}
