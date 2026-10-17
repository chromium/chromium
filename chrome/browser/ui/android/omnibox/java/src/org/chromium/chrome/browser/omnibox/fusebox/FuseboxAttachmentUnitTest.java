// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxTabAttachStartFailureReason;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.utilities.TabLoadingService;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link FuseboxAttachment}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "xhdpi")
public class FuseboxAttachmentUnitTest {
    private static final String CAPTURE_TOKEN = "capture_token";
    private static final String CACHE_TOKEN = "cache_token";
    private static final int TAB_ID = 1;
    private static final String OPTIMIZATION_CANCEL_ON_DESELECTION =
            ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION
                    + ":cancel_load_on_deselection/true";

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Tab mTab;
    @Mock private ComposeboxQueryControllerBridge mBridge;
    @Mock private WebContents mWebContents;
    @Mock private RenderWidgetHostView mRenderWidgetHostView;
    @Mock private TabLoadingService mTabLoadingService;

    @Before
    public void setUp() {
        TabLoadingService.setInstanceForTesting(mTabLoadingService);

        lenient().when(mTab.getTitle()).thenReturn("Tab Title");
        lenient().when(mTab.getId()).thenReturn(TAB_ID);
        // Default to not initialized/frozen/active to test load logic explicitly where needed.
        lenient().when(mTab.isInitialized()).thenReturn(true);
        lenient().when(mTab.isFrozen()).thenReturn(false);
        // By default tab has no WebContents (not active)
        lenient().when(mTab.getWebContents()).thenReturn(null);

        lenient().when(mWebContents.getRenderWidgetHostView()).thenReturn(mRenderWidgetHostView);
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
                        FuseboxAttachmentButtonType.TAB_PICKER,
                        /* isSuggestedTab= */ false);
        when(mBridge.addTabContext(mTab, /* isSuggestedTab= */ false)).thenReturn(CAPTURE_TOKEN);

        boolean result = attachment.uploadToBackend(mBridge, /* bypassTabCacheThisTime= */ false);

        assertTrue(result);
        assertEquals(CAPTURE_TOKEN, attachment.getToken());
        verify(mBridge).addTabContext(mTab, /* isSuggestedTab= */ false);
        verify(mBridge, never()).addTabContextFromCache(anyLong(), anyBoolean());
        verify(mTab, never()).loadIfNeeded(anyBoolean());
    }

    @Test
    public void uploadToBackend_tabAttachment_activeTab_fails() {
        setTabActive(true);
        FuseboxAttachment attachment =
                FuseboxAttachment.forTab(
                        mTab,
                        /* bypassTabCache= */ false,
                        FuseboxAttachmentButtonType.TAB_PICKER,
                        /* isSuggestedTab= */ false);
        when(mBridge.addTabContext(mTab, /* isSuggestedTab= */ false)).thenReturn(null);
        var watcher = expectStartFailure(FuseboxTabAttachStartFailureReason.UPLOAD_REJECTED);

        boolean result = attachment.uploadToBackend(mBridge, /* bypassTabCacheThisTime= */ false);

        assertFalse(result);
        verify(mBridge).addTabContext(mTab, /* isSuggestedTab= */ false);
        watcher.assertExpected();
    }

    @Test
    public void uploadToBackend_inactiveTab_returnsFalseForForcedFetch() {
        setTabActive(false);
        FuseboxAttachment attachment =
                FuseboxAttachment.forTab(
                        mTab,
                        /* bypassTabCache= */ false,
                        FuseboxAttachmentButtonType.TAB_PICKER,
                        /* isSuggestedTab= */ false);
        var watcher =
                expectStartFailure(FuseboxTabAttachStartFailureReason.FRESH_REQUIRED_TAB_NOT_LIVE);

        // Force fetch is true, but capture not allowed and tab not active.
        boolean result = attachment.uploadToBackend(mBridge, /* bypassTabCacheThisTime= */ true);

        assertFalse(result);
        verify(mBridge, never()).addTabContext(any(), anyBoolean());
        verify(mBridge, never()).addTabContextFromCache(anyLong(), anyBoolean());
        watcher.assertExpected();
    }

    @Test
    public void uploadToBackend_inactiveTab_usesCache() {
        setTabActive(false);
        FuseboxAttachment attachment =
                FuseboxAttachment.forTab(
                        mTab,
                        /* bypassTabCache= */ false,
                        FuseboxAttachmentButtonType.TAB_PICKER,
                        /* isSuggestedTab= */ false);
        when(mBridge.addTabContextFromCache(TAB_ID, /* isSuggestedTab= */ false))
                .thenReturn(CACHE_TOKEN);

        // Not forced, background capture disabled. Should try cache.
        boolean result = attachment.uploadToBackend(mBridge, /* bypassTabCacheThisTime= */ false);

        assertTrue(result);
        assertEquals(CACHE_TOKEN, attachment.getToken());
        verify(mBridge).addTabContextFromCache(TAB_ID, /* isSuggestedTab= */ false);
        verify(mBridge, never()).addTabContext(any(), anyBoolean());
    }

    @Test
    public void uploadToBackend_incognitoTab_forcesFreshFetch() {
        setTabActive(true);
        when(mTab.isIncognitoBranded()).thenReturn(true);
        FuseboxAttachment attachment =
                FuseboxAttachment.forTab(
                        mTab,
                        /* bypassTabCache= */ false,
                        FuseboxAttachmentButtonType.TAB_PICKER,
                        /* isSuggestedTab= */ false);
        when(mBridge.addTabContext(mTab, /* isSuggestedTab= */ false)).thenReturn(CAPTURE_TOKEN);

        boolean result = attachment.uploadToBackend(mBridge, /* bypassTabCacheThisTime= */ false);

        assertTrue(result);
        assertEquals(CAPTURE_TOKEN, attachment.getToken());
        verify(mBridge).addTabContext(mTab, /* isSuggestedTab= */ false);
        verify(mBridge, never()).addTabContextFromCache(anyLong(), anyBoolean());
    }

    @Test
    public void retryUpload_tabAttachment_activeTab_succeeds() {
        setTabActive(false);
        FuseboxAttachment attachment =
                FuseboxAttachment.forTab(
                        mTab,
                        /* bypassTabCache= */ false,
                        FuseboxAttachmentButtonType.TAB_PICKER,
                        /* isSuggestedTab= */ false);
        when(mBridge.addTabContextFromCache(TAB_ID, /* isSuggestedTab= */ false))
                .thenReturn(CACHE_TOKEN);
        assertTrue(attachment.uploadToBackend(mBridge, /* bypassTabCacheThisTime= */ false));
        setTabActive(true);
        when(mBridge.addTabContext(mTab, /* isSuggestedTab= */ false)).thenReturn(CAPTURE_TOKEN);

        assertTrue(attachment.retryUpload(mBridge));
        assertEquals(CAPTURE_TOKEN, attachment.getToken());
        verify(mBridge).addTabContext(mTab, /* isSuggestedTab= */ false);
    }

    @Test
    public void retryUpload_tabAttachment_inactiveTab_fails() {
        setTabActive(false);
        FuseboxAttachment attachment =
                FuseboxAttachment.forTab(
                        mTab,
                        /* bypassTabCache= */ false,
                        FuseboxAttachmentButtonType.TAB_PICKER,
                        /* isSuggestedTab= */ false);
        when(mBridge.addTabContextFromCache(TAB_ID, /* isSuggestedTab= */ false))
                .thenReturn(CACHE_TOKEN);
        assertTrue(attachment.uploadToBackend(mBridge, /* bypassTabCacheThisTime= */ false));
        var watcher = expectNoStartFailure();

        assertFalse(attachment.retryUpload(mBridge));
        assertEquals(CACHE_TOKEN, attachment.getToken());
        watcher.assertExpected();
    }

    private FuseboxAttachment createDriveAttachment(@Nullable String resourceKey) {
        DriveAttachmentMetadata metadata =
                new DriveAttachmentMetadata(
                        "drive_id", resourceKey, "title", DriveIconUtils.MIME_TYPE_GOOGLE_DOCS);
        return FuseboxAttachment.forDrive(
                ApplicationProvider.getApplicationContext(),
                metadata,
                TimeUtils.elapsedRealtimeMillis(),
                FuseboxAttachmentButtonType.DRIVE_FILES);
    }

    @Test
    public void uploadToBackend_driveAttachment_success() {
        FuseboxAttachment attachment = createDriveAttachment("resource_key");
        when(mBridge.addDriveFile(
                        "drive_id", "resource_key", "title", DriveIconUtils.MIME_TYPE_GOOGLE_DOCS))
                .thenReturn(CAPTURE_TOKEN);

        assertTrue(attachment.uploadToBackend(mBridge, /* bypassTabCacheThisTime= */ false));
        assertEquals(CAPTURE_TOKEN, attachment.getToken());
    }

    @Test
    public void uploadToBackend_driveAttachment_failure() {
        FuseboxAttachment attachment = createDriveAttachment(/* resourceKey= */ null);
        when(mBridge.addDriveFile("drive_id", null, "title", DriveIconUtils.MIME_TYPE_GOOGLE_DOCS))
                .thenReturn(null);

        assertFalse(attachment.uploadToBackend(mBridge, /* bypassTabCacheThisTime= */ false));
        assertFalse(attachment.hasToken());
    }

    @Test
    @EnableFeatures(OPTIMIZATION_CANCEL_ON_DESELECTION)
    public void removeFromBackend_tabAttachment_cancelsLoadWhenEnabled() {
        FuseboxAttachment attachment =
                FuseboxAttachment.forTab(
                        mTab,
                        /* bypassTabCache= */ false,
                        FuseboxAttachmentButtonType.TAB_PICKER,
                        /* isSuggestedTab= */ false);

        attachment.removeFromBackend(mBridge);

        verify(mTabLoadingService).cancelLoadIfNeeded(mTab);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ON_DEMAND_BACKGROUND_TAB_CONTEXT_CAPTURE_OPTIMIZATION)
    public void removeFromBackend_tabAttachment_doesNotCancelLoadWhenDisabled() {
        FuseboxAttachment attachment =
                FuseboxAttachment.forTab(
                        mTab,
                        /* bypassTabCache= */ false,
                        FuseboxAttachmentButtonType.TAB_PICKER,
                        /* isSuggestedTab= */ false);

        attachment.removeFromBackend(mBridge);

        verify(mTabLoadingService, never()).cancelLoadIfNeeded(any());
    }

    @Test
    @EnableFeatures(OPTIMIZATION_CANCEL_ON_DESELECTION)
    public void removeFromBackend_suggestedTabAttachment_doesNotCancelLoad() {
        FuseboxAttachment attachment =
                FuseboxAttachment.forTab(
                        mTab,
                        /* bypassTabCache= */ false,
                        FuseboxAttachmentButtonType.SUGGESTED_TAB,
                        /* isSuggestedTab= */ true);

        attachment.removeFromBackend(mBridge);

        verify(mTabLoadingService, never()).cancelLoadIfNeeded(any());
    }

    @Test
    public void uploadToBackend_inactiveTabCacheMiss_recordsCacheUnavailableTabNotLive() {
        setTabActive(false);
        when(mBridge.addTabContextFromCache(TAB_ID, /* isSuggestedTab= */ false)).thenReturn(null);
        var watcher =
                expectStartFailure(
                        FuseboxTabAttachStartFailureReason.CACHE_UNAVAILABLE_TAB_NOT_LIVE);

        assertFalse(
                createTabPickerAttachment()
                        .uploadToBackend(mBridge, /* bypassTabCacheThisTime= */ false));
        watcher.assertExpected();
    }

    @Test
    public void uploadToBackend_driveRejected_doesNotRecordTabStartFailure() {
        FuseboxAttachment attachment = createDriveAttachment(/* resourceKey= */ null);
        when(mBridge.addDriveFile(any(), any(), any(), any())).thenReturn(null);
        var watcher = expectNoStartFailure();

        assertFalse(attachment.uploadToBackend(mBridge, /* bypassTabCacheThisTime= */ false));
        watcher.assertExpected();
    }

    @Test
    public void retryUpload_afterSuccess_resetsCompleteButKeepsSucceeded() {
        FuseboxAttachment attachment = createCachedTabAttachment();
        attachment.markUploadSucceeded();
        setTabActive(true);
        when(mBridge.addTabContext(mTab, /* isSuggestedTab= */ false)).thenReturn(CAPTURE_TOKEN);

        assertTrue(attachment.retryUpload(mBridge));
        assertFalse(attachment.isUploadComplete());
        assertTrue(attachment.hasUploadSucceeded());
    }

    @Test
    public void retryUpload_rejectedAfterSuccess_leavesUploadComplete() {
        FuseboxAttachment attachment = createCachedTabAttachment();
        attachment.markUploadSucceeded();

        // Inactive tab cannot do the forced fresh fetch a retry requires.
        assertFalse(attachment.retryUpload(mBridge));
        assertTrue(attachment.isUploadComplete());
    }

    private FuseboxAttachment createTabPickerAttachment() {
        return FuseboxAttachment.forTab(
                mTab,
                /* bypassTabCache= */ false,
                FuseboxAttachmentButtonType.TAB_PICKER,
                /* isSuggestedTab= */ false);
    }

    /** Creates an attachment for an inactive tab whose upload started from the tab cache. */
    private FuseboxAttachment createCachedTabAttachment() {
        setTabActive(false);
        when(mBridge.addTabContextFromCache(TAB_ID, /* isSuggestedTab= */ false))
                .thenReturn(CACHE_TOKEN);
        FuseboxAttachment attachment = createTabPickerAttachment();
        assertTrue(attachment.uploadToBackend(mBridge, /* bypassTabCacheThisTime= */ false));
        return attachment;
    }

    private static HistogramWatcher expectStartFailure(
            @FuseboxTabAttachStartFailureReason int reason) {
        return HistogramWatcher.newBuilder()
                .expectIntRecord(
                        FuseboxMetrics.TAB_ATTACHMENT_START_FAILURE_REASON_HISTOGRAM, reason)
                .expectIntRecord(
                        FuseboxMetrics.TAB_ATTACHMENT_START_FAILURE_REASON_HISTOGRAM + ".TabPicker",
                        reason)
                .build();
    }

    private static HistogramWatcher expectNoStartFailure() {
        return HistogramWatcher.newBuilder()
                .expectNoRecords(FuseboxMetrics.TAB_ATTACHMENT_START_FAILURE_REASON_HISTOGRAM)
                .build();
    }
}
