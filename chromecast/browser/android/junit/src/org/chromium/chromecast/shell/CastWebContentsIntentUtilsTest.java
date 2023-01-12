// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.net.Uri;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContents;

/**
 * Tests for CastWebContentsComponent.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CastWebContentsIntentUtilsTest {
    private static final String EXPECTED_URI = "cast://webcontents/123-abc";
    private static final String APP_ID = "app";
    private static final String SESSION_ID = "123-abc";
    private static final int VISIBILITY_PRIORITY = 2;

    private @Mock WebContents mWebContents;
    private @Mock BroadcastReceiver mReceiver;
    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Mockito.spy(Robolectric.buildActivity(Activity.class).setup().get());
    }

    @Test
    public void testOnActivityStopped() {
        Intent in = CastWebContentsIntentUtils.onActivityStopped(SESSION_ID);
        String uri = in.getDataString();
        Assert.assertNotNull(uri);
        Assert.assertEquals(EXPECTED_URI, uri);
        Assert.assertTrue(CastWebContentsIntentUtils.isIntentOfActivityStopped(in));
    }

    @Test
    public void testOnVisibilityChange() {
        Intent in = CastWebContentsIntentUtils.onVisibilityChange(SESSION_ID, 3);
        String uri = in.getDataString();
        Assert.assertNotNull(uri);
        Assert.assertEquals(EXPECTED_URI, uri);
        int type = CastWebContentsIntentUtils.getVisibilityType(in);
        Assert.assertEquals(3, type);
        Assert.assertTrue(CastWebContentsIntentUtils.isIntentOfVisibilityChange(in));
    }

    @Test
    public void testRequestStartCastActivity() {
        Intent in = CastWebContentsIntentUtils.requestStartCastActivity(
                mActivity, mWebContents, true, true, true, false, SESSION_ID);
        Assert.assertTrue(CastWebContentsIntentUtils.shouldRequestAudioFocus(in));
        Assert.assertNull(in.getData());
        String uri = CastWebContentsIntentUtils.getUriString(in);
        Assert.assertNotNull(uri);
        Assert.assertEquals(EXPECTED_URI, uri);
        WebContents webContents = CastWebContentsIntentUtils.getWebContents(in);
        Assert.assertEquals(mWebContents, webContents);
        Assert.assertTrue(CastWebContentsIntentUtils.isTouchable(in));
        Assert.assertEquals(Intent.ACTION_VIEW, in.getAction());
    }

    @Test
    public void testRequestStartCastService() {
        Intent in = CastWebContentsIntentUtils.requestStartCastService(
                mActivity, mWebContents, SESSION_ID);
        String uri = in.getDataString();
        Assert.assertNotNull(uri);
        Assert.assertEquals(EXPECTED_URI, uri);
        WebContents webContents = CastWebContentsIntentUtils.getWebContents(in);
        Assert.assertEquals(mWebContents, webContents);
        Assert.assertEquals(Intent.ACTION_VIEW, in.getAction());
    }

    @Test
    public void testRequestStopWebContents() {
        Intent in = CastWebContentsIntentUtils.requestStopWebContents(SESSION_ID);
        String uri = CastWebContentsIntentUtils.getUriString(in);
        Assert.assertNotNull(uri);
        Assert.assertEquals(EXPECTED_URI, uri);
    }

    @Test
    public void testEnableTouchInputTrue() {
        Intent in = CastWebContentsIntentUtils.enableTouchInput(SESSION_ID, true);
        String uri = CastWebContentsIntentUtils.getUriString(in);
        Assert.assertNotNull(uri);
        Assert.assertEquals(EXPECTED_URI, uri);
        Assert.assertEquals(CastWebContentsIntentUtils.ACTION_ENABLE_TOUCH_INPUT, in.getAction());
        Assert.assertTrue(CastWebContentsIntentUtils.isTouchable(in));
    }

    @Test
    public void testEnableTouchInputFalse() {
        Intent in = CastWebContentsIntentUtils.enableTouchInput(SESSION_ID, false);
        String uri = CastWebContentsIntentUtils.getUriString(in);
        Assert.assertNotNull(uri);
        Assert.assertEquals(EXPECTED_URI, uri);
        Assert.assertEquals(CastWebContentsIntentUtils.ACTION_ENABLE_TOUCH_INPUT, in.getAction());
        Assert.assertFalse(CastWebContentsIntentUtils.isTouchable(in));
    }

    @Test
    public void testShouldTurnOnScreenActivityTrue() {
        Intent intent = CastWebContentsIntentUtils.requestStartCastActivity(
                mActivity, mWebContents, true, false, true, false, SESSION_ID);
        Assert.assertTrue(CastWebContentsIntentUtils.shouldTurnOnScreen(intent));
    }

    @Test
    public void testShouldTurnOnScreenActivityFalse() {
        Intent intent = CastWebContentsIntentUtils.requestStartCastActivity(
                mActivity, mWebContents, true, false, false, false, SESSION_ID);
        Assert.assertFalse(CastWebContentsIntentUtils.shouldTurnOnScreen(intent));
    }

    @Test
    public void testOnWebContentStopped() {
        Intent in = CastWebContentsIntentUtils.onWebContentStopped(Uri.parse(EXPECTED_URI));
        String uri = CastWebContentsIntentUtils.getUriString(in);
        Assert.assertNotNull(uri);
        Assert.assertEquals(EXPECTED_URI, uri);
    }

    @Test
    public void testMediaPlaying() {
        Intent in0 = CastWebContentsIntentUtils.mediaPlaying(SESSION_ID, true);
        Intent in1 = CastWebContentsIntentUtils.mediaPlaying(SESSION_ID, false);
        String uri0 = CastWebContentsIntentUtils.getUriString(in0);
        String uri1 = CastWebContentsIntentUtils.getUriString(in0);
        Assert.assertNotNull(uri0);
        Assert.assertNotNull(uri1);
        Assert.assertEquals(EXPECTED_URI, uri0);
        Assert.assertEquals(EXPECTED_URI, uri1);
        Assert.assertEquals(CastWebContentsIntentUtils.ACTION_MEDIA_PLAYING, in0.getAction());
        Assert.assertEquals(CastWebContentsIntentUtils.ACTION_MEDIA_PLAYING, in1.getAction());
        Assert.assertTrue(CastWebContentsIntentUtils.isMediaPlaying(in0));
        Assert.assertFalse(CastWebContentsIntentUtils.isMediaPlaying(in1));
    }

    @Test
    public void testRequestMediaPlayingStatus() {
        Intent in = CastWebContentsIntentUtils.requestMediaPlayingStatus(SESSION_ID);
        Assert.assertEquals(
                CastWebContentsIntentUtils.ACTION_REQUEST_MEDIA_PLAYING_STATUS, in.getAction());
        Assert.assertTrue(in.toURI().startsWith(EXPECTED_URI));
    }
}
