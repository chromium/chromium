// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Robolectric.buildService;
import static org.robolectric.Shadows.shadowOf;

import android.app.Application;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.PatternMatcher;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.android.controller.ServiceController;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.ContextUtils;
import org.chromium.content_public.browser.WebContents;

/**
 * Tests for TaskRemovedMonitorService
 */
@RunWith(RobolectricTestRunner.class)
@LooperMode(Mode.PAUSED)
public class TaskRemovedMonitorServiceTest {
    @Rule
    public MockitoRule mockitoRule = MockitoJUnit.rule();

    private final String mRootId = "1234";
    private final String mSessionId = "5678";

    private Application mContext;
    private ServiceController<TaskRemovedMonitorService> mController;
    private Service mTaskRemovedMonitorService;

    @Mock
    private WebContents mWebContents;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        ContextUtils.initApplicationContextForTests(mContext);
        Intent startIntent = new Intent(mContext, TaskRemovedMonitorService.class);
        startIntent.putExtra(TaskRemovedMonitorService.ROOT_SESSION_KEY, mRootId);
        startIntent.putExtra(TaskRemovedMonitorService.SESSION_KEY, mSessionId);
        mController = buildService(TaskRemovedMonitorService.class, startIntent);
        mTaskRemovedMonitorService = mController.get();
    }

    @Test
    public void testStartStartsTaskRemovedMonitorService() {
        TaskRemovedMonitorService.start(mRootId, mSessionId);
        Intent serviceIntent = shadowOf(mContext).getNextStartedService();
        assertNotNull(serviceIntent);
        assertEquals(TaskRemovedMonitorService.class.getName(),
                serviceIntent.getComponent().getClassName());
        assertEquals(
                mRootId, serviceIntent.getStringExtra(TaskRemovedMonitorService.ROOT_SESSION_KEY));
        assertEquals(
                mSessionId, serviceIntent.getStringExtra(TaskRemovedMonitorService.SESSION_KEY));
    }

    @Test
    public void testStopStopsTaskRemovedMonitorService() {
        String root = "foo";
        String session = "bar";
        TaskRemovedMonitorService.start(mRootId, mSessionId);
        TaskRemovedMonitorService.stop();
        Intent serviceIntent = shadowOf(mContext).getNextStoppedService();
        assertNotNull(serviceIntent);
        assertEquals(TaskRemovedMonitorService.class.getName(),
                serviceIntent.getComponent().getClassName());
    }

    @Test
    public void testOnTaskRemovedForDifferentComponentIsIgnored() {
        mController.create();
        mController.startCommand(0, 0);
        Intent taskRemovedIntent = new Intent(mContext, TaskRemovedMonitorServiceTest.class);
        verifyBroadcastedIntent(
                filterFor(CastWebContentsIntentUtils.ACTION_ACTIVITY_STOPPED), () -> {
                    mTaskRemovedMonitorService.onTaskRemoved(taskRemovedIntent);
                    assertFalse(shadowOf(mTaskRemovedMonitorService).isStoppedBySelf());
                }, false);
    }

    @Test
    public void testOnTaskRemovedIgnoredIfRootIdDoesNotMatch() {
        mController.create();
        mController.startCommand(0, 0);
        verifyBroadcastedIntent(
                filterFor(CastWebContentsIntentUtils.ACTION_ACTIVITY_STOPPED), () -> {
                    Intent taskRemovedIntent = getIntentForSession("foo_session_id");
                    mTaskRemovedMonitorService.onTaskRemoved(taskRemovedIntent);
                    assertFalse(shadowOf(mTaskRemovedMonitorService).isStoppedBySelf());
                }, false);
    }

    @Test
    public void testOnTaskRemovedStopsSessionIfRootIdMatches() {
        mController.create();
        mController.startCommand(0, 0);
        verifyBroadcastedIntent(
                filterFor(CastWebContentsIntentUtils.ACTION_ACTIVITY_STOPPED), () -> {
                    Intent taskRemovedIntent = getIntentForSession(mRootId);
                    mTaskRemovedMonitorService.onTaskRemoved(taskRemovedIntent);
                    assertTrue(shadowOf(mTaskRemovedMonitorService).isStoppedBySelf());
                }, true);
    }

    private void verifyBroadcastedIntent(
            IntentFilter filter, Runnable runnable, boolean shouldExpect) {
        BroadcastReceiver receiver = mock(BroadcastReceiver.class);
        LocalBroadcastManager.getInstance(mContext).registerReceiver(receiver, filter);
        try {
            runnable.run();
        } finally {
            LocalBroadcastManager.getInstance(mContext).unregisterReceiver(receiver);
            if (shouldExpect) {
                verify(receiver).onReceive(any(Context.class), any(Intent.class));
            } else {
                verify(receiver, times(0)).onReceive(any(Context.class), any(Intent.class));
            }
        }
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

    private Intent getIntentForSession(String sessionId) {
        return CastWebContentsIntentUtils.requestStartCastActivity(mContext, mWebContents,
                /* enableTouch= */ true, /* shouldRequestAudioFocus= */ false,
                /* turnOnScreen= */ false, /* keepScreenOn */ false, sessionId);
    }
}
