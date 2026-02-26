// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.res.Resources;
import android.graphics.Bitmap;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link FuseboxAttachment}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, qualifiers = "xhdpi")
public class FuseboxAttachmentUnitTest {
    private static final String CAPTURE_TOKEN = "capture_token";
    private static final String CACHE_TOKEN = "cache_token";
    private static final int TAB_ID = 1;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private ComposeboxQueryControllerBridge mBridge;
    @Mock private WebContents mWebContents;
    @Mock private RenderWidgetHostView mRenderWidgetHostView;

    private Resources mResources;
    private Bitmap mBitmap;

    @Before
    public void setUp() {
        mResources = ApplicationProvider.getApplicationContext().getResources();
        mBitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        OmniboxResourceProvider.setTabFaviconFactory((tab) -> mBitmap);

        when(mTab.getTitle()).thenReturn("Tab Title");
        when(mTab.getId()).thenReturn(TAB_ID);
        // Default to not initialized/frozen/active to test load logic explicitly where needed.
        when(mTab.isInitialized()).thenReturn(true);
        when(mTab.isFrozen()).thenReturn(false);
        // By default tab has no WebContents (not active)
        when(mTab.getWebContents()).thenReturn(null);

        when(mWebContents.getRenderWidgetHostView()).thenReturn(mRenderWidgetHostView);
    }

    private void setTabActive(boolean active) {
        if (active) {
            when(mTab.getWebContents()).thenReturn(mWebContents);
        } else {
            when(mTab.getWebContents()).thenReturn(null);
        }
    }

    @Test
    public void uploadToBackend_tabAttachment_activeTab() {
        setTabActive(true);
        FuseboxAttachment attachment =
                FuseboxAttachment.forTab(
                        mTab,
                        /* bypassTabCache= */ true,
                        mResources,
                        FuseboxAttachmentButtonType.TAB_PICKER);
        when(mBridge.addTabContext(mTab)).thenReturn(CAPTURE_TOKEN);

        boolean result = attachment.uploadToBackend(mBridge, false);

        assertTrue(result);
        assertEquals(CAPTURE_TOKEN, attachment.getToken());
        verify(mBridge).addTabContext(mTab);
        verify(mBridge, never()).addTabContextFromCache(anyLong());
        verify(mTab, never()).loadIfNeeded(anyInt());
    }

    @Test
    public void uploadToBackend_tabAttachment_activeTab_fails() {
        setTabActive(true);
        FuseboxAttachment attachment =
                FuseboxAttachment.forTab(
                        mTab,
                        /* bypassTabCache= */ false,
                        mResources,
                        FuseboxAttachmentButtonType.TAB_PICKER);
        when(mBridge.addTabContext(mTab)).thenReturn(null);

        boolean result = attachment.uploadToBackend(mBridge, false);

        assertFalse(result);
        verify(mBridge).addTabContext(mTab);
    }

    @Test
    public void uploadToBackend_inactiveTab_returnsFalseForForcedFetch() {
        setTabActive(false);
        FuseboxAttachment attachment =
                FuseboxAttachment.forTab(
                        mTab,
                        /* bypassTabCache= */ false,
                        mResources,
                        FuseboxAttachmentButtonType.TAB_PICKER);

        // Force fetch is true, but capture not allowed and tab not active.
        boolean result = attachment.uploadToBackend(mBridge, true);

        assertFalse(result);
        verify(mBridge, never()).addTabContext(any());
        verify(mBridge, never()).addTabContextFromCache(anyLong());
    }

    @Test
    public void uploadToBackend_inactiveTab_usesCache() {
        setTabActive(false);
        FuseboxAttachment attachment =
                FuseboxAttachment.forTab(
                        mTab,
                        /* bypassTabCache= */ false,
                        mResources,
                        FuseboxAttachmentButtonType.TAB_PICKER);
        when(mBridge.addTabContextFromCache(TAB_ID)).thenReturn(CACHE_TOKEN);

        // Not forced, background capture disabled. Should try cache.
        boolean result = attachment.uploadToBackend(mBridge, false);

        assertTrue(result);
        assertEquals(CACHE_TOKEN, attachment.getToken());
        verify(mBridge).addTabContextFromCache(TAB_ID);
        verify(mBridge, never()).addTabContext(any());
    }

    @Test
    public void uploadToBackend_incognitoTab_forcesFreshFetch() {
        setTabActive(true);
        when(mTab.isIncognitoBranded()).thenReturn(true);
        FuseboxAttachment attachment =
                FuseboxAttachment.forTab(
                        mTab,
                        /* bypassTabCache= */ false,
                        mResources,
                        FuseboxAttachmentButtonType.TAB_PICKER);
        when(mBridge.addTabContext(mTab)).thenReturn(CAPTURE_TOKEN);

        boolean result = attachment.uploadToBackend(mBridge, false);

        assertTrue(result);
        assertEquals(CAPTURE_TOKEN, attachment.getToken());
        verify(mBridge).addTabContext(mTab);
        verify(mBridge, never()).addTabContextFromCache(anyLong());
    }
}
