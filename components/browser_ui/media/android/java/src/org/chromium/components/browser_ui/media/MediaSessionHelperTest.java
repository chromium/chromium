// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.withSettings;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.IntentFilter;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/** Unit tests for {@link MediaSessionHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MediaSessionHelperTest {
    @Mock private MediaSessionHelper.Delegate mDelegate;
    private WebContents mWebContents;
    @Mock private Context mContext;
    @Mock private MediaSession mMediaSession;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mWebContents =
                mock(
                        WebContents.class,
                        withSettings().extraInterfaces(WebContentsObserver.Observable.class));
        ContextUtils.initApplicationContextForTests(mContext);
        MediaSessionHelper.sOverriddenMediaSession = mMediaSession;
    }

    @After
    public void tearDown() {
        MediaSessionHelper.sOverriddenMediaSession = null;
    }

    @Test
    public void testReceiverLifecycle_Shared() {
        // First instance creation should register the receiver.
        MediaSessionHelper helper1 = new MediaSessionHelper(mWebContents, mDelegate);
        verify(mContext, times(1))
                .registerReceiver(
                        any(BroadcastReceiver.class),
                        any(IntentFilter.class),
                        eq(null),
                        eq(null),
                        eq(0));

        // Second instance should NOT register a new receiver.
        MediaSessionHelper helper2 = new MediaSessionHelper(mWebContents, mDelegate);
        verify(mContext, times(1))
                .registerReceiver(
                        any(BroadcastReceiver.class),
                        any(IntentFilter.class),
                        eq(null),
                        eq(null),
                        eq(0));

        // Destroying the first instance should NOT unregister the receiver yet.
        helper1.destroy();
        verify(mContext, never()).unregisterReceiver(any(BroadcastReceiver.class));

        // Destroying the last instance SHOULD unregister the receiver.
        helper2.destroy();
        verify(mContext, times(1)).unregisterReceiver(any(BroadcastReceiver.class));
    }
}
