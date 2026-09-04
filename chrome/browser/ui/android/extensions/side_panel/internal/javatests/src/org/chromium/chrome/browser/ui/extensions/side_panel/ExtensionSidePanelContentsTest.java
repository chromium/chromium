// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.side_panel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link ExtensionSidePanelContents}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionSidePanelContentsTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContents mWebContents;
    @Mock private ThinWebView mThinWebView;
    @Mock private View mThinWebViewUnderlyingView;

    private Activity mActivity;
    private IntentRequestTracker mIntentRequestTracker;
    private ActivityWindowAndroid mWindowAndroid;

    @Before
    public void setUp() {
        ExtensionSidePanelContents.setSkipViewEventSinkForTesting(true);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mIntentRequestTracker = IntentRequestTracker.createFromActivity(mActivity);
        mWindowAndroid =
                new ActivityWindowAndroid(
                        mActivity,
                        /* listenToActivityState= */ false,
                        mIntentRequestTracker,
                        /* insetObserver= */ null,
                        /* occlusionTrackingAllowed= */ false);
        ThinWebViewFactory.setInstanceForTesting(mThinWebView);
    }

    @After
    public void tearDown() {
        if (mWindowAndroid != null) {
            mWindowAndroid.destroy();
        }
    }

    @Test
    public void testCreate_nullContext_returnsNull() {
        WindowAndroid mockWindow = mock(WindowAndroid.class);
        when(mockWindow.getContext()).thenReturn(new WeakReference<Context>(null));

        ExtensionSidePanelContents contents =
                ExtensionSidePanelContents.create(mWebContents, mockWindow);

        assertNull(contents);
    }

    @Test
    public void testCreate_nullIntentRequestTracker_returnsNull() {
        WindowAndroid windowWithoutTracker =
                new WindowAndroid(mActivity, /* occlusionTrackingAllowed= */ false);

        ExtensionSidePanelContents contents =
                ExtensionSidePanelContents.create(mWebContents, windowWithoutTracker);

        assertNull(contents);
        windowWithoutTracker.destroy();
    }

    @Test
    public void testCreate_success_setsDelegatesAndAttachesThinWebView() {
        when(mThinWebView.getView()).thenReturn(mThinWebViewUnderlyingView);
        when(mWebContents.getViewAndroidDelegate()).thenReturn(null);

        ExtensionSidePanelContents contents =
                ExtensionSidePanelContents.create(mWebContents, mWindowAndroid);

        assertNotNull(contents);
        assertEquals(mWebContents, contents.getWebContents());
        assertEquals(mThinWebViewUnderlyingView, contents.getView());

        verify(mWebContents)
                .setDelegates(
                        any(),
                        any(ViewAndroidDelegate.class),
                        any(ContentView.class),
                        eq(mWindowAndroid),
                        any());
        verify(mThinWebView)
                .attachWebContents(
                        eq(mWebContents),
                        any(ContentView.class),
                        any(ThinWebViewAttachParams.class));
    }

    @Test
    public void testCreate_existingDelegate_updatesWindowAndContainer() {
        ViewAndroidDelegate delegate = ViewAndroidDelegate.createBasicDelegate(null);
        when(mWebContents.getViewAndroidDelegate()).thenReturn(delegate);

        ExtensionSidePanelContents contents =
                ExtensionSidePanelContents.create(mWebContents, mWindowAndroid);

        assertNotNull(contents);
        verify(mWebContents).setTopLevelNativeWindow(mWindowAndroid);
        assertNotNull(delegate.getContainerView());
    }

    @Test
    public void testDestroy_destroysThinWebViewAndResetsNativeWindow() {
        ViewAndroidDelegate delegate = ViewAndroidDelegate.createBasicDelegate(null);
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getViewAndroidDelegate()).thenReturn(delegate);

        ExtensionSidePanelContents contents =
                ExtensionSidePanelContents.create(mWebContents, mWindowAndroid);
        assertNotNull(contents);
        assertNotNull(delegate.getContainerView());

        contents.destroy();

        verify(mThinWebView).destroy();
        verify(mWebContents).setTopLevelNativeWindow(null);
        assertNull(delegate.getContainerView());
    }

    @Test
    public void testDestroy_webContentsAlreadyDestroyed_onlyDestroysThinWebView() {
        when(mWebContents.isDestroyed()).thenReturn(true);

        ExtensionSidePanelContents contents =
                ExtensionSidePanelContents.create(mWebContents, mWindowAndroid);
        assertNotNull(contents);

        contents.destroy();

        verify(mThinWebView).destroy();
        verify(mWebContents, never()).setTopLevelNativeWindow(any());
    }
}
