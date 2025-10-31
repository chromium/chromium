// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Intent;
import android.net.Uri;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContents;

/** Tests for CastWebContentsComponent. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CastWebContentsIntentUtilsTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String EXPECTED_URI = "cast://webcontents/123-abc";
    private static final String APP_ID = "app";
    private static final String SESSION_ID = "123-abc";
    private static final int VISIBILITY_PRIORITY = 2;

    private @Mock WebContents mWebContents;

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
        Intent in =
                CastWebContentsIntentUtils.requestStartCastActivity(
                        mWebContents, true, true, true, false, SESSION_ID);
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
        Intent intent =
                CastWebContentsIntentUtils.requestStartCastActivity(
                        mWebContents, true, false, true, false, SESSION_ID);
        Assert.assertTrue(CastWebContentsIntentUtils.shouldTurnOnScreen(intent));
    }

    @Test
    public void testShouldTurnOnScreenActivityFalse() {
        Intent intent =
                CastWebContentsIntentUtils.requestStartCastActivity(
                        mWebContents, true, false, false, false, SESSION_ID);
        Assert.assertFalse(CastWebContentsIntentUtils.shouldTurnOnScreen(intent));
    }

    @Test
    public void testOnWebContentStopped() {
        Intent in = CastWebContentsIntentUtils.onWebContentStopped(Uri.parse(EXPECTED_URI));
        String uri = CastWebContentsIntentUtils.getUriString(in);
        Assert.assertNotNull(uri);
        Assert.assertEquals(EXPECTED_URI, uri);
    }
}
