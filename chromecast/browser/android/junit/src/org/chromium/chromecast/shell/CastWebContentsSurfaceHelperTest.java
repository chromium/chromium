// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Intent;
import android.net.Uri;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Consumer;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chromecast.base.Observer;
import org.chromium.chromecast.base.Scope;
import org.chromium.chromecast.shell.CastWebContentsSurfaceHelper.MediaSessionGetter;
import org.chromium.chromecast.shell.CastWebContentsSurfaceHelper.StartParams;
import org.chromium.content.browser.MediaSessionImpl;
import org.chromium.content_public.browser.WebContents;

/**
 * Tests for CastWebContentsSurfaceHelper.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CastWebContentsSurfaceHelperTest {
    private @Mock Observer<WebContents> mWebContentsView;
    private @Mock Consumer<Uri> mFinishCallback;
    private CastWebContentsSurfaceHelper mSurfaceHelper;
    private @Mock MediaSessionGetter mMediaSessionGetter;
    private @Mock MediaSessionImpl mMediaSessionImpl;

    private static class StartParamsBuilder {
        private String mId = "0";
        private WebContents mWebContents = mock(WebContents.class);
        private boolean mIsRemoteControlMode;
        private boolean mIsTouchInputEnabled;

        public StartParamsBuilder withId(String id) {
            mId = id;
            return this;
        }

        public StartParamsBuilder withWebContents(WebContents webContents) {
            mWebContents = webContents;
            return this;
        }

        public StartParamsBuilder withIsRemoteControlMode(boolean isRemoteControlMode) {
            mIsRemoteControlMode = isRemoteControlMode;
            return this;
        }

        public StartParamsBuilder enableTouchInput(boolean enableTouchInput) {
            mIsTouchInputEnabled = enableTouchInput;
            return this;
        }

        public StartParams build() {
            return new StartParams(CastWebContentsIntentUtils.getInstanceUri(mId), mWebContents,
                    mIsRemoteControlMode, mIsTouchInputEnabled);
        }
    }

    private void sendBroadcastSync(Intent intent) {
        CastWebContentsIntentUtils.getLocalBroadcastManager().sendBroadcastSync(intent);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMediaSessionGetter.get(any())).thenReturn(mMediaSessionImpl);
        when(mWebContentsView.open(any())).thenReturn(mock(Scope.class));
        mSurfaceHelper = new CastWebContentsSurfaceHelper(mWebContentsView, mFinishCallback);
        mSurfaceHelper.setMediaSessionGetterForTesting(mMediaSessionGetter);
    }

    @Test
    public void testActivatesWebContentsViewOnNewStartParams() {
        WebContents webContents = mock(WebContents.class);
        StartParams params = new StartParamsBuilder().withWebContents(webContents).build();
        mSurfaceHelper.onNewStartParams(params);
        verify(mWebContentsView).open(webContents);
    }

    @Test
    public void testRequestsAudioFocusOnNewStartParams() {
        WebContents webContents = mock(WebContents.class);
        StartParams params = new StartParamsBuilder().withWebContents(webContents).build();
        mSurfaceHelper.onNewStartParams(params);
        verify(mMediaSessionImpl).requestSystemAudioFocus();
    }

    @Test
    public void testDoesNotTakeAudioFocusInRemoteControlMode() {
        WebContents webContents = mock(WebContents.class);
        StartParams params = new StartParamsBuilder()
                                     .withId("3")
                                     .withWebContents(webContents)
                                     .withIsRemoteControlMode(true)
                                     .build();
        mSurfaceHelper.onNewStartParams(params);
        verify(mMediaSessionImpl, never()).requestSystemAudioFocus();
    }

    @Test
    public void testDeactivatesOldWebContentsViewOnNewStartParams() {
        WebContents webContents1 = mock(WebContents.class);
        StartParams params1 =
                new StartParamsBuilder().withId("1").withWebContents(webContents1).build();
        WebContents webContents2 = mock(WebContents.class);
        StartParams params2 =
                new StartParamsBuilder().withId("2").withWebContents(webContents2).build();
        Scope scope1 = mock(Scope.class);
        Scope scope2 = mock(Scope.class);
        when(mWebContentsView.open(webContents1)).thenReturn(scope1);
        when(mWebContentsView.open(webContents2)).thenReturn(scope2);
        mSurfaceHelper.onNewStartParams(params1);
        verify(mWebContentsView).open(webContents1);
        mSurfaceHelper.onNewStartParams(params2);
        verify(scope1).close();
        verify(mWebContentsView).open(webContents2);
    }

    @Test
    public void testDoesNotRestartWebContentsIfNewStartParamsHasSameWebContents() {
        WebContents webContents = mock(WebContents.class);
        // Create two StartParams that have the same WebContents but different values.
        StartParams params1 = new StartParamsBuilder()
                                      .withId("1")
                                      .withWebContents(webContents)
                                      .enableTouchInput(false)
                                      .build();
        StartParams params2 = new StartParamsBuilder()
                                      .withId("1")
                                      .withWebContents(webContents)
                                      .enableTouchInput(true)
                                      .build();
        Scope scope = mock(Scope.class);
        when(mWebContentsView.open(webContents)).thenReturn(scope);
        mSurfaceHelper.onNewStartParams(params1);
        // The second StartParams has the same WebContents, so we shouldn't open the scopes again.
        mSurfaceHelper.onNewStartParams(params2);
        verify(mWebContentsView, times(1)).open(webContents);
    }

    @Test
    public void testIsTouchInputEnabled() {
        assertFalse(mSurfaceHelper.isTouchInputEnabled());
        StartParams params1 = new StartParamsBuilder().enableTouchInput(true).build();
        mSurfaceHelper.onNewStartParams(params1);
        assertTrue(mSurfaceHelper.isTouchInputEnabled());
        StartParams params2 = new StartParamsBuilder().enableTouchInput(false).build();
        mSurfaceHelper.onNewStartParams(params2);
        assertFalse(mSurfaceHelper.isTouchInputEnabled());
    }

    @Test
    public void testSessionId() {
        assertNull(mSurfaceHelper.getSessionId());
        StartParams params1 = new StartParamsBuilder().withId("/abc123").build();
        mSurfaceHelper.onNewStartParams(params1);
        assertEquals("/abc123", mSurfaceHelper.getSessionId());
        StartParams params2 = new StartParamsBuilder().withId("/123-abc").build();
        mSurfaceHelper.onNewStartParams(params2);
        assertEquals("/123-abc", mSurfaceHelper.getSessionId());
    }

    @Test
    public void testScreenOffResetsWebContentsView() {
        WebContents webContents = mock(WebContents.class);
        StartParams params = new StartParamsBuilder().withWebContents(webContents).build();
        Scope scope = mock(Scope.class);
        when(mWebContentsView.open(webContents)).thenReturn(scope);
        mSurfaceHelper.onNewStartParams(params);
        // Send SCREEN_OFF broadcast.
        sendBroadcastSync(new Intent(CastIntents.ACTION_SCREEN_OFF));
        verify(scope).close();
    }

    @Test
    public void testStopWebContentsIntentResetsWebContentsView() {
        WebContents webContents = mock(WebContents.class);
        StartParams params =
                new StartParamsBuilder().withId("3").withWebContents(webContents).build();
        Scope scope = mock(Scope.class);
        when(mWebContentsView.open(webContents)).thenReturn(scope);
        mSurfaceHelper.onNewStartParams(params);
        // Send notification to stop web content
        sendBroadcastSync(CastWebContentsIntentUtils.requestStopWebContents("3"));
        verify(scope).close();
    }

    @Test
    public void testStopWebContentsIntentWithWrongIdIsIgnored() {
        WebContents webContents = mock(WebContents.class);
        StartParams params =
                new StartParamsBuilder().withId("2").withWebContents(webContents).build();
        Scope scope = mock(Scope.class);
        when(mWebContentsView.open(webContents)).thenReturn(scope);
        mSurfaceHelper.onNewStartParams(params);
        // Send notification to stop web content with different ID.
        sendBroadcastSync(CastWebContentsIntentUtils.requestStopWebContents("4"));
        verify(scope, never()).close();
    }

    @Test
    public void testEnableTouchInputIntentMutatesIsTouchInputEnabled() {
        WebContents webContents = mock(WebContents.class);
        StartParams params = new StartParamsBuilder().withId("1").enableTouchInput(false).build();
        mSurfaceHelper.onNewStartParams(params);
        assertFalse(mSurfaceHelper.isTouchInputEnabled());
        // Send broadcast to enable touch input.
        sendBroadcastSync(CastWebContentsIntentUtils.enableTouchInput("1", true));
        assertTrue(mSurfaceHelper.isTouchInputEnabled());
    }

    @Test
    public void testEnableTouchInputIntentWithWrongIdIsIgnored() {
        WebContents webContents = mock(WebContents.class);
        StartParams params = new StartParamsBuilder().withId("1").enableTouchInput(false).build();
        mSurfaceHelper.onNewStartParams(params);
        assertFalse(mSurfaceHelper.isTouchInputEnabled());
        // Send broadcast to enable touch input with different ID.
        sendBroadcastSync(CastWebContentsIntentUtils.enableTouchInput("2", true));
        assertFalse(mSurfaceHelper.isTouchInputEnabled());
    }

    @Test
    public void testDisableTouchInputIntent() {
        WebContents webContents = mock(WebContents.class);
        StartParams params = new StartParamsBuilder().withId("1").enableTouchInput(true).build();
        mSurfaceHelper.onNewStartParams(params);
        assertTrue(mSurfaceHelper.isTouchInputEnabled());
        // Send broadcast to enable touch input.
        sendBroadcastSync(CastWebContentsIntentUtils.enableTouchInput("1", false));
        assertFalse(mSurfaceHelper.isTouchInputEnabled());
    }

    @Test
    public void testFinishLaterCallbackIsRunAfterStopWebContents() {
        StartParams params = new StartParamsBuilder().withId("0").build();
        mSurfaceHelper.onNewStartParams(params);
        sendBroadcastSync(CastWebContentsIntentUtils.requestStopWebContents("0"));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mFinishCallback).accept(CastWebContentsIntentUtils.getInstanceUri("0"));
    }

    @Test
    public void testFinishLaterCallbackIsRunAfterScreenOff() {
        StartParams params = new StartParamsBuilder().withId("0").build();
        mSurfaceHelper.onNewStartParams(params);
        sendBroadcastSync(new Intent(CastIntents.ACTION_SCREEN_OFF));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mFinishCallback).accept(CastWebContentsIntentUtils.getInstanceUri("0"));
    }

    @Test
    public void testFinishLaterCallbackIsNotRunIfNewWebContentsIsReceived() {
        StartParams params1 = new StartParamsBuilder().withId("1").build();
        StartParams params2 = new StartParamsBuilder().withId("2").build();
        mSurfaceHelper.onNewStartParams(params1);
        sendBroadcastSync(CastWebContentsIntentUtils.requestStopWebContents("1"));
        mSurfaceHelper.onNewStartParams(params2);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mFinishCallback, never()).accept(any());
    }

    @Test
    public void testOnDestroyClosesWebContentsView() {
        WebContents webContents = mock(WebContents.class);
        Scope scope = mock(Scope.class);
        StartParams params = new StartParamsBuilder().withWebContents(webContents).build();
        when(mWebContentsView.open(webContents)).thenReturn(scope);
        mSurfaceHelper.onNewStartParams(params);
        mSurfaceHelper.onDestroy();
        verify(scope).close();
    }
}
