// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.os.SystemClock;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxTabAttachStartFailureReason;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.utilities.TabLoadingService;
import org.chromium.components.contextual_search.ContextUploadErrorType;
import org.chromium.components.contextual_search.ContextUploadStatus;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.ListObservable.ListObserver;

import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;

@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxAttachmentModelListUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private ComposeboxQueryControllerBridge mComposeboxQueryControllerBridge;
    @Mock private FuseboxAttachmentModelList.FuseboxAttachmentChangeListener mListener;
    @Mock private Tab mTab;
    @Mock private Runnable mAttachmentUploadFailedListener;
    @Mock private WebContents mWebContents;
    @Mock private RenderWidgetHostView mRenderWidgetHostView;
    @Mock private TabLoadingService mTabLoadingService;
    @Mock private ListObserver<Void> mListObserver;

    private FuseboxAttachmentModelList mFuseboxAttachmentModelList;

    private FuseboxAttachment createTestAttachment(String title) {
        return FuseboxAttachment.forFile(
                /* thumbnail= */ null,
                title + ".txt",
                "mime/" + title,
                title.getBytes(),
                SystemClock.elapsedRealtime(),
                FuseboxAttachmentButtonType.FILES);
    }

    @Before
    public void setUp() {
        TabLoadingService.setInstanceForTesting(mTabLoadingService);
        mFuseboxAttachmentModelList = new FuseboxAttachmentModelList();
        mFuseboxAttachmentModelList.setComposeboxQueryControllerBridge(
                mComposeboxQueryControllerBridge);
        verify(mComposeboxQueryControllerBridge)
                .setContextUploadObserver(mFuseboxAttachmentModelList);
        mFuseboxAttachmentModelList.addAttachmentChangeListener(mListener);
        mFuseboxAttachmentModelList.setAttachmentUploadFailedListener(
                mAttachmentUploadFailedListener);
    }

    private FuseboxAttachment createTabAttachment(Tab tab) {
        return FuseboxAttachment.forTab(
                tab,
                /* bypassTabCache= */ false,
                FuseboxAttachmentButtonType.TAB_PICKER,
                /* isSuggestedTab= */ false);
    }

    private FuseboxAttachment createTabAttachment(int tabId, String token) {
        return createTabAttachment(
                tabId, token, FuseboxAttachmentButtonType.TAB_PICKER, /* isSuggestedTab= */ false);
    }

    private FuseboxAttachment createSuggestedTabAttachment(int tabId, String token) {
        return createTabAttachment(
                tabId,
                token,
                FuseboxAttachmentButtonType.SUGGESTED_TAB,
                /* isSuggestedTab= */ true);
    }

    /**
     * Creates a tab attachment for a distinct inactive tab whose cached-context lookup yields
     * token.
     */
    private FuseboxAttachment createTabAttachment(
            int tabId,
            String token,
            @FuseboxAttachmentButtonType int buttonType,
            boolean isSuggestedTab) {
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(tabId);
        when(mComposeboxQueryControllerBridge.addTabContextFromCache(tabId, isSuggestedTab))
                .thenReturn(token);
        return FuseboxAttachment.forTab(
                tab, /* bypassTabCache= */ false, buttonType, isSuggestedTab);
    }

    @Test
    public void testAdd_withValidToken_startsSessionAndAddsItem() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn("valid-token");

        assertFalse(mFuseboxAttachmentModelList.isSessionStarted());
        FuseboxAttachment attachment = createTestAttachment("test");
        mFuseboxAttachmentModelList.add(attachment);

        verify(mComposeboxQueryControllerBridge).notifySessionStarted();
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("test.txt"), eq("mime/test"), eq("test".getBytes()));
        assertEquals(1, mFuseboxAttachmentModelList.size());
        assertTrue(mFuseboxAttachmentModelList.isSessionStarted());
        assertEquals("valid-token", attachment.getToken());
        assertFalse(attachment.isUploadComplete());

        verifyNoMoreInteractions(mComposeboxQueryControllerBridge);
    }

    @Test
    public void testAdd_withInvalidToken_doesNotAddItem() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn(null);

        FuseboxAttachment attachment = createTestAttachment("test");
        mFuseboxAttachmentModelList.add(attachment);

        verify(mComposeboxQueryControllerBridge).notifySessionStarted();
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("test.txt"), eq("mime/test"), eq("test".getBytes()));
        verify(mComposeboxQueryControllerBridge).notifySessionAbandoned();
        assertEquals(0, mFuseboxAttachmentModelList.size());
        assertFalse(mFuseboxAttachmentModelList.isSessionStarted());
        verifyNoMoreInteractions(mComposeboxQueryControllerBridge);
    }

    @Test
    public void testAdd_withInvalidToken_notifiesUploadFailed() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn(null);
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                FuseboxMetrics.TAB_ATTACHMENT_START_FAILURE_REASON_HISTOGRAM)
                        .build();

        FuseboxAttachment attachment = createTestAttachment("test");
        mFuseboxAttachmentModelList.add(attachment);

        verify(mAttachmentUploadFailedListener).run();
        watcher.assertExpected();
    }

    @Test
    public void testAdd_withEmptyToken_doesNotAddItem() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn("");

        FuseboxAttachment attachment = createTestAttachment("test");
        mFuseboxAttachmentModelList.add(attachment);

        verify(mComposeboxQueryControllerBridge).notifySessionStarted();
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("test.txt"), eq("mime/test"), eq("test".getBytes()));
        verify(mComposeboxQueryControllerBridge).notifySessionAbandoned();
        assertEquals(0, mFuseboxAttachmentModelList.size());
        assertFalse(mFuseboxAttachmentModelList.isSessionStarted());
        verifyNoMoreInteractions(mComposeboxQueryControllerBridge);
    }

    @Test
    public void testAdd_withNullBridge_doesNotAddItem() {
        FuseboxAttachment attachment = createTestAttachment("test");

        mFuseboxAttachmentModelList.setComposeboxQueryControllerBridge(null);
        verify(mComposeboxQueryControllerBridge).setContextUploadObserver(null);

        assertFalse(mFuseboxAttachmentModelList.isSessionStarted());
        mFuseboxAttachmentModelList.add(attachment);

        assertEquals(0, mFuseboxAttachmentModelList.size());
        assertFalse(mFuseboxAttachmentModelList.isSessionStarted());
        verify(mComposeboxQueryControllerBridge, never()).addFile(anyString(), anyString(), any());
        verify(mComposeboxQueryControllerBridge, never()).notifySessionStarted();
        verifyNoMoreInteractions(mComposeboxQueryControllerBridge);
    }

    @Test
    public void testRemove_withValidToken_removesFromBackendAndAbandonsSession() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn("test-token");
        FuseboxAttachment attachment = createTestAttachment("test");
        mFuseboxAttachmentModelList.add(attachment);

        mFuseboxAttachmentModelList.remove(attachment, /* isFailure= */ false);
        verify(mComposeboxQueryControllerBridge).notifySessionStarted();
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("test.txt"), eq("mime/test"), eq("test".getBytes()));
        verify(mComposeboxQueryControllerBridge).removeAttachment("test-token");
        verify(mComposeboxQueryControllerBridge).notifySessionAbandoned();
        assertEquals(0, mFuseboxAttachmentModelList.size());
        assertFalse(mFuseboxAttachmentModelList.isSessionStarted());
        verifyNoMoreInteractions(mComposeboxQueryControllerBridge);
    }

    @Test
    public void testRemove_multipleItems_doesNotAbandonSessionUntilEmpty() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn("token1", "token2");
        FuseboxAttachment firstAttachment = createTestAttachment("first");
        FuseboxAttachment secondAttachment = createTestAttachment("second");
        mFuseboxAttachmentModelList.add(firstAttachment);
        mFuseboxAttachmentModelList.add(secondAttachment);

        mFuseboxAttachmentModelList.remove(firstAttachment, /* isFailure= */ false);
        verify(mComposeboxQueryControllerBridge, never()).notifySessionAbandoned();
        assertTrue(mFuseboxAttachmentModelList.isSessionStarted());

        mFuseboxAttachmentModelList.remove(secondAttachment, /* isFailure= */ false);
        verify(mComposeboxQueryControllerBridge).notifySessionStarted();
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("first.txt"), eq("mime/first"), eq("first".getBytes()));
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("second.txt"), eq("mime/second"), eq("second".getBytes()));
        verify(mComposeboxQueryControllerBridge).removeAttachment("token1");
        verify(mComposeboxQueryControllerBridge).removeAttachment("token2");
        verify(mComposeboxQueryControllerBridge).notifySessionAbandoned();
        assertFalse(mFuseboxAttachmentModelList.isSessionStarted());
        verifyNoMoreInteractions(mComposeboxQueryControllerBridge);
    }

    @Test
    public void testClear_removesAllIndividuallyAndAbandonsSession() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn("token1", "token2");
        FuseboxAttachment firstAttachment = createTestAttachment("first");
        FuseboxAttachment secondAttachment = createTestAttachment("second");

        mFuseboxAttachmentModelList.add(firstAttachment);
        mFuseboxAttachmentModelList.add(secondAttachment);

        mFuseboxAttachmentModelList.clear();

        verify(mComposeboxQueryControllerBridge).notifySessionStarted();
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("first.txt"), eq("mime/first"), eq("first".getBytes()));
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("second.txt"), eq("mime/second"), eq("second".getBytes()));
        verify(mComposeboxQueryControllerBridge).removeAttachment("token1");
        verify(mComposeboxQueryControllerBridge).removeAttachment("token2");
        verify(mComposeboxQueryControllerBridge).notifySessionAbandoned();
        assertEquals(0, mFuseboxAttachmentModelList.size());
        assertFalse(mFuseboxAttachmentModelList.isSessionStarted());
        verifyNoMoreInteractions(mComposeboxQueryControllerBridge);
    }

    @Test
    public void testSessionLifecycle_multipleAdditions_onlyStartsSessionOnce() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn("token1", "token2", "token3");

        FuseboxAttachment attachment1 = createTestAttachment("first");
        FuseboxAttachment attachment2 = createTestAttachment("second");
        FuseboxAttachment attachment3 = createTestAttachment("third");

        mFuseboxAttachmentModelList.add(attachment1);
        mFuseboxAttachmentModelList.add(attachment2);
        mFuseboxAttachmentModelList.add(attachment3);

        verify(mComposeboxQueryControllerBridge).notifySessionStarted();
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("first.txt"), eq("mime/first"), eq("first".getBytes()));
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("second.txt"), eq("mime/second"), eq("second".getBytes()));
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("third.txt"), eq("mime/third"), eq("third".getBytes()));
        assertEquals(3, mFuseboxAttachmentModelList.size());
        assertTrue(mFuseboxAttachmentModelList.isSessionStarted());
        verifyNoMoreInteractions(mComposeboxQueryControllerBridge);
    }

    @Test
    public void testRemove_withNullBridge_stillRemovesFromUIAndBackend() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn("test-token");
        FuseboxAttachment attachment = createTestAttachment("test");
        mFuseboxAttachmentModelList.add(attachment);
        assertEquals(1, mFuseboxAttachmentModelList.size());

        mFuseboxAttachmentModelList.setComposeboxQueryControllerBridge(null);
        verify(mComposeboxQueryControllerBridge).setContextUploadObserver(null);

        assertEquals(0, mFuseboxAttachmentModelList.size());
        verify(mComposeboxQueryControllerBridge).notifySessionStarted();
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("test.txt"), eq("mime/test"), eq("test".getBytes()));
        verify(mComposeboxQueryControllerBridge).removeAttachment("test-token");
        verify(mComposeboxQueryControllerBridge).notifySessionAbandoned();
        verifyNoMoreInteractions(mComposeboxQueryControllerBridge);
    }

    @Test
    public void testDestroy_removesAttachmentsFromBackend() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn("test-token");
        FuseboxAttachment attachment = createTestAttachment("test");
        mFuseboxAttachmentModelList.add(attachment);
        assertEquals(1, mFuseboxAttachmentModelList.size());

        mFuseboxAttachmentModelList.destroy();
        verify(mComposeboxQueryControllerBridge).setContextUploadObserver(null);

        assertEquals(0, mFuseboxAttachmentModelList.size());
        verify(mComposeboxQueryControllerBridge).notifySessionStarted();
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("test.txt"), eq("mime/test"), eq("test".getBytes()));
        verify(mComposeboxQueryControllerBridge).removeAttachment("test-token");
        verify(mComposeboxQueryControllerBridge).notifySessionAbandoned();
        verifyNoMoreInteractions(mComposeboxQueryControllerBridge);
    }

    @Test
    public void testClear_mixedAttachmentTypes_removesAllProperly() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn("file-token", "tab-token");

        FuseboxAttachment fileAttachment = createTestAttachment("file");
        FuseboxAttachment tabAttachment = createTestAttachment("tab");

        mFuseboxAttachmentModelList.add(fileAttachment);
        mFuseboxAttachmentModelList.add(tabAttachment);

        mFuseboxAttachmentModelList.clear();

        verify(mComposeboxQueryControllerBridge).notifySessionStarted();
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("file.txt"), eq("mime/file"), eq("file".getBytes()));
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("tab.txt"), eq("mime/tab"), eq("tab".getBytes()));
        verify(mComposeboxQueryControllerBridge).removeAttachment("file-token");
        verify(mComposeboxQueryControllerBridge).removeAttachment("tab-token");
        verify(mComposeboxQueryControllerBridge).notifySessionAbandoned();
        assertEquals(0, mFuseboxAttachmentModelList.size());
        assertFalse(mFuseboxAttachmentModelList.isSessionStarted());
        verifyNoMoreInteractions(mComposeboxQueryControllerBridge);
    }

    @Test
    public void testAddAttachment_needsTokenUpload_uploadsAndSucceeds() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn("uploaded-token");

        assertFalse(mFuseboxAttachmentModelList.isSessionStarted());
        FuseboxAttachment attachment = createTestAttachment("test");
        mFuseboxAttachmentModelList.add(attachment);

        verify(mComposeboxQueryControllerBridge).notifySessionStarted();
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("test.txt"), eq("mime/test"), eq("test".getBytes()));
        assertEquals(1, mFuseboxAttachmentModelList.size());
        assertTrue(mFuseboxAttachmentModelList.isSessionStarted());
        assertEquals("uploaded-token", attachment.getToken());
        assertFalse(attachment.isUploadComplete());

        try (var ignored =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.ContextUploadStatus",
                                ContextUploadStatus.UPLOAD_SUCCESSFUL)
                        .build()) {
            mFuseboxAttachmentModelList.onContextUploadStatusChanged(
                    "uploaded-token",
                    ContextUploadStatus.UPLOAD_SUCCESSFUL,
                    ContextUploadErrorType.UNKNOWN);
        }
        assertTrue(attachment.isUploadComplete());
        verifyNoMoreInteractions(mComposeboxQueryControllerBridge);
    }

    @Test
    public void testAddAttachment_uploadFails_doesNotAdd() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn(null);

        FuseboxAttachment attachment = createTestAttachment("test");
        mFuseboxAttachmentModelList.add(attachment);

        verify(mComposeboxQueryControllerBridge).notifySessionStarted();
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("test.txt"), eq("mime/test"), eq("test".getBytes()));
        verify(mComposeboxQueryControllerBridge).notifySessionAbandoned();
        assertEquals(0, mFuseboxAttachmentModelList.size());
        assertFalse(mFuseboxAttachmentModelList.isSessionStarted());
        verifyNoMoreInteractions(mComposeboxQueryControllerBridge);
    }

    @Test
    public void testAddAttachment_mixedScenarios_handlesAllCorrectly() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn("pretokenized-token", "uploaded-token");

        FuseboxAttachment preTokenizedAttachment = createTestAttachment("pretokenized");
        FuseboxAttachment uploadedAttachment = createTestAttachment("uploaded");
        AtomicBoolean uploadFailedNotified = new AtomicBoolean(false);
        mFuseboxAttachmentModelList.setAttachmentUploadFailedListener(
                () -> uploadFailedNotified.set(true));
        mFuseboxAttachmentModelList.add(preTokenizedAttachment);
        mFuseboxAttachmentModelList.add(uploadedAttachment);
        assertEquals(2, mFuseboxAttachmentModelList.size());
        assertTrue(mFuseboxAttachmentModelList.isSessionStarted());
        verify(mComposeboxQueryControllerBridge).notifySessionStarted();
        verify(mComposeboxQueryControllerBridge)
                .addFile(
                        eq("pretokenized.txt"),
                        eq("mime/pretokenized"),
                        eq("pretokenized".getBytes()));
        verify(mComposeboxQueryControllerBridge)
                .addFile(eq("uploaded.txt"), eq("mime/uploaded"), eq("uploaded".getBytes()));
        assertEquals("pretokenized-token", preTokenizedAttachment.getToken());
        assertEquals("uploaded-token", uploadedAttachment.getToken());

        assertFalse(preTokenizedAttachment.isUploadComplete());
        assertFalse(uploadedAttachment.isUploadComplete());

        try (var ignored =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.ContextUploadStatus",
                                ContextUploadStatus.UPLOAD_SUCCESSFUL)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.ContextUploadStatus",
                                ContextUploadStatus.UPLOAD_FAILED)
                        .expectIntRecord(
                                "Omnibox.MobileFusebox.ContextUploadError",
                                ContextUploadErrorType.UNKNOWN)
                        .build()) {
            mFuseboxAttachmentModelList.onContextUploadStatusChanged(
                    "pretokenized-token",
                    ContextUploadStatus.UPLOAD_SUCCESSFUL,
                    ContextUploadErrorType.UNKNOWN);
            mFuseboxAttachmentModelList.onContextUploadStatusChanged(
                    "uploaded-token",
                    ContextUploadStatus.UPLOAD_FAILED,
                    ContextUploadErrorType.UNKNOWN);
        }
        assertTrue(uploadFailedNotified.get());

        assertTrue(preTokenizedAttachment.isUploadComplete());
        assertTrue(uploadedAttachment.isUploadComplete());

        assertEquals(0, mFuseboxAttachmentModelList.indexOf(preTokenizedAttachment));
        assertEquals(-1, mFuseboxAttachmentModelList.indexOf(uploadedAttachment));
    }

    @Test
    public void testMaxAttachments() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn("pretokenized-token", "uploaded-token");
        for (int i = 0; i < FuseboxAttachmentModelList.MAX_ATTACHMENTS; i++) {
            FuseboxAttachment attachment = createTestAttachment("pretokenized");
            assertTrue(mFuseboxAttachmentModelList.add(attachment));
        }

        FuseboxAttachment attachment = createTestAttachment("pretokenized");
        assertFalse(mFuseboxAttachmentModelList.add(attachment));
    }

    @Test
    public void testAdd_tabAttachment_tracksTabId() {
        FuseboxAttachment attachment = createTabAttachment(1, "tab-token-1");
        assertEquals(FuseboxAttachmentType.ATTACHMENT_TAB, attachment.type);
        mFuseboxAttachmentModelList.add(attachment);
        assertTrue(mFuseboxAttachmentModelList.getAttachedTabIds().contains(1));
    }

    @Test
    public void testRemove_tabAttachment_untracksTabId() {
        FuseboxAttachment attachment = createTabAttachment(1, "tab-token-1");
        mFuseboxAttachmentModelList.add(attachment);
        mFuseboxAttachmentModelList.remove(attachment, /* isFailure= */ false);
        assertFalse(mFuseboxAttachmentModelList.getAttachedTabIds().contains(1));
    }

    @Test
    public void testClear_mixedAttachments_clearsAllTabIds() {
        FuseboxAttachment tab1 = createTabAttachment(1, "tab-token-1");
        FuseboxAttachment file = createTestAttachment("file");
        FuseboxAttachment tab2 = createTabAttachment(2, "tab-token-2");

        mFuseboxAttachmentModelList.add(tab1);
        mFuseboxAttachmentModelList.add(file);
        mFuseboxAttachmentModelList.add(tab2);
        assertEquals(2, mFuseboxAttachmentModelList.getAttachedTabIds().size());

        mFuseboxAttachmentModelList.clear();
        assertTrue(mFuseboxAttachmentModelList.getAttachedTabIds().isEmpty());
    }

    @Test
    public void testCurrentTabDirectlyFetchesContext() {
        doReturn(1).when(mTab).getId();
        doReturn(true).when(mTab).isInitialized();
        doReturn(false).when(mTab).isFrozen();
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mRenderWidgetHostView).when(mWebContents).getRenderWidgetHostView();
        when(mComposeboxQueryControllerBridge.addTabContext(mTab, /* isSuggestedTab= */ false))
                .thenReturn("token");

        FuseboxAttachment tabAttachment = createTabAttachment(mTab);
        mFuseboxAttachmentModelList.add(tabAttachment);

        assertEquals("token", tabAttachment.getToken());
    }

    @Test
    public void testIncognitoTabDoesNotUseCache() {
        doReturn(1).when(mTab).getId();
        doReturn(true).when(mTab).isInitialized();
        doReturn(false).when(mTab).isFrozen();
        doReturn(true).when(mTab).isIncognitoBranded();
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mRenderWidgetHostView).when(mWebContents).getRenderWidgetHostView();
        when(mComposeboxQueryControllerBridge.addTabContext(mTab, /* isSuggestedTab= */ false))
                .thenReturn("token");

        FuseboxAttachment tabAttachment = createTabAttachment(mTab);
        mFuseboxAttachmentModelList.add(tabAttachment);

        assertEquals("token", tabAttachment.getToken());
    }

    @Test
    public void testRetryTabUpload_failedImmediately() {
        doReturn(1).when(mTab).getId();
        doReturn(true).when(mTab).isInitialized();
        doReturn(false).when(mTab).isFrozen();
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mRenderWidgetHostView).when(mWebContents).getRenderWidgetHostView();
        when(mComposeboxQueryControllerBridge.addTabContext(mTab, /* isSuggestedTab= */ false))
                .thenReturn("token2");
        when(mComposeboxQueryControllerBridge.addTabContextFromCache(
                        1, /* isSuggestedTab= */ false))
                .thenReturn("");

        FuseboxAttachment tabAttachment = createTabAttachment(mTab);
        mFuseboxAttachmentModelList.add(tabAttachment);
        assertEquals("token2", tabAttachment.getToken());
    }

    @Test
    public void testRetryTabUpload_failedAfterTokenGenerated() {
        doReturn(1).when(mTab).getId();
        doReturn(true).when(mTab).isInitialized();
        doReturn(false).when(mTab).isFrozen();
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mRenderWidgetHostView).when(mWebContents).getRenderWidgetHostView();
        when(mComposeboxQueryControllerBridge.addTabContextFromCache(
                        1, /* isSuggestedTab= */ false))
                .thenReturn("token");

        FuseboxAttachment tabAttachment = createTabAttachment(mTab);
        mFuseboxAttachmentModelList.add(tabAttachment);
        assertEquals("token", tabAttachment.getToken());

        when(mComposeboxQueryControllerBridge.addTabContext(mTab, /* isSuggestedTab= */ false))
                .thenReturn("token2");
        mFuseboxAttachmentModelList.onContextUploadStatusChanged(
                "token", ContextUploadStatus.VALIDATION_FAILED, ContextUploadErrorType.UNKNOWN);
        assertEquals("token2", tabAttachment.getToken());

        mFuseboxAttachmentModelList.onContextUploadStatusChanged(
                "token2", ContextUploadStatus.VALIDATION_FAILED, ContextUploadErrorType.UNKNOWN);
        assertTrue(mFuseboxAttachmentModelList.isEmpty());
    }

    @Test
    public void batchEdit_singleAdd_notifiesChangeOnce() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn("valid-token");

        try (var token = mFuseboxAttachmentModelList.beginBatchEdit()) {
            FuseboxAttachment attachment = createTestAttachment("test");
            mFuseboxAttachmentModelList.add(attachment);
            verify(mListener, never()).onAttachmentListChanged();
        }

        verify(mListener).onAttachmentListChanged();
        assertEquals(1, mFuseboxAttachmentModelList.size());
    }

    @Test
    public void batchEdit_multipleAdds_notifiesChangeOnce() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn("token1", "token2");

        try (var token = mFuseboxAttachmentModelList.beginBatchEdit()) {
            mFuseboxAttachmentModelList.add(createTestAttachment("first"));
            mFuseboxAttachmentModelList.add(createTestAttachment("second"));
            verify(mListener, never()).onAttachmentListChanged();
        }

        verify(mListener).onAttachmentListChanged();
        assertEquals(2, mFuseboxAttachmentModelList.size());
    }

    @Test
    public void batchEdit_noModifications_doesNotNotifyChange() {
        try (var token = mFuseboxAttachmentModelList.beginBatchEdit()) {
            // No modifications
        }
        verify(mListener, never()).onAttachmentListChanged();
    }

    @Test
    public void batchEdit_nestedEdits_notifiesChangeOnceAtTheEnd() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn("token1", "token2");

        try (var outerToken = mFuseboxAttachmentModelList.beginBatchEdit()) {
            mFuseboxAttachmentModelList.add(createTestAttachment("first"));

            try (var innerToken = mFuseboxAttachmentModelList.beginBatchEdit()) {
                mFuseboxAttachmentModelList.add(createTestAttachment("second"));
            }
            verify(mListener, never()).onAttachmentListChanged();
        }

        verify(mListener).onAttachmentListChanged();
        assertEquals(2, mFuseboxAttachmentModelList.size());
    }

    @Test
    public void batchEdit_addAndRemove_notifiesChangeOnce() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn("token1", "token2");
        FuseboxAttachment attachment1 = createTestAttachment("first");
        FuseboxAttachment attachment2 = createTestAttachment("second");

        // Add an attachment before starting the batch edit.
        mFuseboxAttachmentModelList.add(attachment1);
        // Clear invocations on listener to ignore the notification from the previous add.
        clearInvocations(mListener);

        try (var token = mFuseboxAttachmentModelList.beginBatchEdit()) {
            mFuseboxAttachmentModelList.add(attachment2);
            mFuseboxAttachmentModelList.remove(attachment1, /* isFailure= */ false);
            verify(mListener, never()).onAttachmentListChanged();
        }

        verify(mListener).onAttachmentListChanged();
        assertEquals(1, mFuseboxAttachmentModelList.size());
        assertEquals(attachment2, mFuseboxAttachmentModelList.get(0));
    }

    @Test
    public void testNotifyAttachmentSucceeded() {
        try (var ignored =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Omnibox.MobileFusebox.AttachmentSucceeded")
                        .expectAnyRecord("Omnibox.MobileFusebox.AttachmentSucceeded.Files")
                        .build()) {
            when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                    .thenReturn("token1");
            FuseboxAttachment attachment = createTestAttachment("test");
            mFuseboxAttachmentModelList.add(attachment);
            mFuseboxAttachmentModelList.onContextUploadStatusChanged(
                    "token1",
                    ContextUploadStatus.UPLOAD_SUCCESSFUL,
                    ContextUploadErrorType.UNKNOWN);
        }
    }

    @Test
    public void testNotifyAttachmentFailed() {
        try (var ignored =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Omnibox.MobileFusebox.AttachmentFailed")
                        .expectAnyRecord("Omnibox.MobileFusebox.AttachmentFailed.Files")
                        .build()) {
            when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                    .thenReturn("token1");
            FuseboxAttachment attachment = createTestAttachment("test");
            mFuseboxAttachmentModelList.add(attachment);
            mFuseboxAttachmentModelList.remove(attachment, /* isFailure= */ true);
        }
    }

    @Test
    public void testNotifyAttachmentAbandoned() {
        try (var ignored =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Omnibox.MobileFusebox.AttachmentAbandoned")
                        .expectAnyRecord("Omnibox.MobileFusebox.AttachmentAbandoned.Files")
                        .build()) {
            when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                    .thenReturn("token1");
            FuseboxAttachment attachment = createTestAttachment("test");
            mFuseboxAttachmentModelList.add(attachment);
            mFuseboxAttachmentModelList.remove(attachment, /* isFailure= */ false);
        }
    }

    @Test
    public void testRemoveTabsNotInSet() {
        mFuseboxAttachmentModelList.add(createTabAttachment(/* tabId= */ 1, "tab-token-1"));
        mFuseboxAttachmentModelList.add(createTabAttachment(/* tabId= */ 2, "tab-token-1"));
        mFuseboxAttachmentModelList.add(createTabAttachment(/* tabId= */ 3, "tab-token-1"));
        assertThat(mFuseboxAttachmentModelList.getAttachedTabIds()).containsExactly(1, 2, 3);

        mFuseboxAttachmentModelList.removeTabsNotInSet(Set.of(1, 3, 5));
        assertThat(mFuseboxAttachmentModelList.getAttachedTabIds()).containsExactly(1, 3);

        mFuseboxAttachmentModelList.removeTabsNotInSet(Set.of());
        assertThat(mFuseboxAttachmentModelList.getAttachedTabIds()).isEmpty();
    }

    @Test
    public void testRetryTabUpload_failedSynchronously_removesAttachment() {
        doReturn(1).when(mTab).getId();
        doReturn(true).when(mTab).isInitialized();
        doReturn(false).when(mTab).isFrozen();
        doReturn(null).when(mTab).getWebContents();
        when(mComposeboxQueryControllerBridge.addTabContextFromCache(
                        1, /* isSuggestedTab= */ false))
                .thenReturn("token");
        FuseboxAttachment tabAttachment = createTabAttachment(mTab);
        mFuseboxAttachmentModelList.add(tabAttachment);

        mFuseboxAttachmentModelList.onContextUploadStatusChanged(
                "token", ContextUploadStatus.VALIDATION_FAILED, ContextUploadErrorType.UNKNOWN);

        assertTrue(mFuseboxAttachmentModelList.isEmpty());
        verify(mComposeboxQueryControllerBridge).removeAttachment("token");
    }

    private FuseboxAttachment addTabPickerAttachment(int tabId, String token, long loadDurationMs) {
        FuseboxAttachment attachment = createTabAttachment(tabId, token);
        mFuseboxAttachmentModelList.add(attachment);
        when(mTabLoadingService.getAndClearLoadDuration(tabId)).thenReturn(loadDurationMs);
        return attachment;
    }

    private void notifyUploadSuccessful(String token) {
        mFuseboxAttachmentModelList.onContextUploadStatusChanged(
                token, ContextUploadStatus.UPLOAD_SUCCESSFUL, ContextUploadErrorType.UNKNOWN);
    }

    private HistogramWatcher newEffectiveDurationWatcher(
            String recordedSuffix, String skippedSuffix) {
        return HistogramWatcher.newBuilder()
                .expectAnyRecord(
                        FuseboxMetrics.TAB_ATTACHMENT_EFFECTIVE_DURATION_HISTOGRAM + recordedSuffix)
                .expectNoRecords(
                        FuseboxMetrics.TAB_ATTACHMENT_EFFECTIVE_DURATION_HISTOGRAM + skippedSuffix)
                .build();
    }

    @Test
    public void testOnContextUploadStatusChanged_tabPickerAttachment_recordsEffectiveDuration() {
        addTabPickerAttachment(/* tabId= */ 1, "tab-token-1", /* loadDurationMs= */ 500L);
        var watcher = newEffectiveDurationWatcher(".SingleTab", ".MultipleTabs");

        notifyUploadSuccessful("tab-token-1");

        watcher.assertExpected();
    }

    @Test
    public void testOnContextUploadStatusChanged_multiTabSelection_recordsMultipleTabsDuration() {
        addTabPickerAttachment(/* tabId= */ 2, "tab-token-2", /* loadDurationMs= */ 1000L)
                .setIsPartOfMultiTabSelection(true);
        var watcher = newEffectiveDurationWatcher(".MultipleTabs", ".SingleTab");

        notifyUploadSuccessful("tab-token-2");

        watcher.assertExpected();
    }

    @Test
    public void testOnContextUploadStatusChanged_warmTab_recordsEffectiveDuration() {
        addTabPickerAttachment(/* tabId= */ 1, "tab-token-warm", /* loadDurationMs= */ 0L);
        var watcher = newEffectiveDurationWatcher(".SingleTab", ".MultipleTabs");

        notifyUploadSuccessful("tab-token-warm");

        watcher.assertExpected();
    }

    @Test
    public void testAdd_limitReached_recordsAttachmentLimitReached() {
        when(mComposeboxQueryControllerBridge.addFile(anyString(), anyString(), any()))
                .thenReturn("token-file");
        for (int i = 0; i < FuseboxAttachmentModelList.MAX_ATTACHMENTS; i++) {
            mFuseboxAttachmentModelList.add(createTestAttachment("file" + i));
        }

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                FuseboxMetrics.TAB_ATTACHMENT_START_FAILURE_REASON_HISTOGRAM,
                                FuseboxTabAttachStartFailureReason.ATTACHMENT_LIMIT_REACHED)
                        .expectIntRecord(
                                FuseboxMetrics.TAB_ATTACHMENT_START_FAILURE_REASON_HISTOGRAM
                                        + ".TabPicker",
                                FuseboxTabAttachStartFailureReason.ATTACHMENT_LIMIT_REACHED)
                        .build();

        assertFalse(mFuseboxAttachmentModelList.add(createTabAttachment(mTab)));
        watcher.assertExpected();
    }

    @Test
    public void testAdd_nullBridge_recordsSessionUnavailable() {
        mFuseboxAttachmentModelList.setComposeboxQueryControllerBridge(null);
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                FuseboxMetrics.TAB_ATTACHMENT_START_FAILURE_REASON_HISTOGRAM,
                                FuseboxTabAttachStartFailureReason.SESSION_UNAVAILABLE)
                        .build();

        assertFalse(mFuseboxAttachmentModelList.add(createTabAttachment(mTab)));
        watcher.assertExpected();
    }

    @Test
    public void testAdd_tabUploadRejected_recordsStartFailureReason() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                FuseboxMetrics.TAB_ATTACHMENT_START_FAILURE_REASON_HISTOGRAM,
                                FuseboxTabAttachStartFailureReason.CACHE_UNAVAILABLE_TAB_NOT_LIVE)
                        .build();

        assertFalse(
                mFuseboxAttachmentModelList.add(
                        createTabAttachment(/* tabId= */ 1, /* token= */ null)));
        watcher.assertExpected();
    }

    @Test
    public void testOnContextUploadStatusChanged_uploadExpiredAfterSuccess_doesNotRecordFailed() {
        addTabPickerAttachment(/* tabId= */ 1, "tab-token-1", /* loadDurationMs= */ 0L);
        notifyUploadSuccessful("tab-token-1");

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Omnibox.MobileFusebox.AttachmentFailed")
                        .expectNoRecords("Omnibox.MobileFusebox.AttachmentAbandoned")
                        .build();

        mFuseboxAttachmentModelList.onContextUploadStatusChanged(
                "tab-token-1", ContextUploadStatus.UPLOAD_EXPIRED, ContextUploadErrorType.UNKNOWN);
        watcher.assertExpected();
        assertTrue(mFuseboxAttachmentModelList.isEmpty());
    }

    @Test
    public void testOnContextUploadStatusChanged_retryAfterExpiry_doesNotRecordSucceededAgain() {
        addActiveTabUploadedFromCache();
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Omnibox.MobileFusebox.AttachmentSucceeded")
                        .expectNoRecords(
                                FuseboxMetrics.TAB_ATTACHMENT_EFFECTIVE_DURATION_HISTOGRAM
                                        + ".SingleTab")
                        .build();

        expireUpload("token-cache");
        notifyUploadSuccessful("token-fresh");
        watcher.assertExpected();
    }

    /**
     * Adds an active tab whose upload started from the cache as "token-cache" and succeeded; a
     * retry will capture it fresh as "token-fresh".
     */
    private FuseboxAttachment addActiveTabUploadedFromCache() {
        setUpActiveTab(1);
        when(mComposeboxQueryControllerBridge.addTabContextFromCache(
                        /* tabId= */ 1, /* isSuggestedTab= */ false))
                .thenReturn("token-cache");
        when(mComposeboxQueryControllerBridge.addTabContext(mTab, /* isSuggestedTab= */ false))
                .thenReturn("token-fresh");
        var attachment = createTabAttachment(mTab);
        mFuseboxAttachmentModelList.add(attachment);
        notifyUploadSuccessful("token-cache");
        return attachment;
    }

    private void expireUpload(String token) {
        mFuseboxAttachmentModelList.onContextUploadStatusChanged(
                token, ContextUploadStatus.UPLOAD_EXPIRED, ContextUploadErrorType.UNKNOWN);
    }

    private void setUpActiveTab(int tabId) {
        doReturn(tabId).when(mTab).getId();
        doReturn(true).when(mTab).isInitialized();
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mRenderWidgetHostView).when(mWebContents).getRenderWidgetHostView();
    }

    private HistogramWatcher newNoAbandonedOrFailedWatcher() {
        return HistogramWatcher.newBuilder()
                .expectNoRecords("Omnibox.MobileFusebox.AttachmentAbandoned")
                .expectNoRecords("Omnibox.MobileFusebox.AttachmentFailed")
                .build();
    }

    @Test
    public void testRemove_retryAfterSuccessInFlight_doesNotRecordAbandoned() {
        var attachment = addActiveTabUploadedFromCache();
        expireUpload("token-cache");
        var watcher = newNoAbandonedOrFailedWatcher();

        mFuseboxAttachmentModelList.remove(attachment, /* isFailure= */ false);

        watcher.assertExpected();
    }

    @Test
    public void testClear_retryAfterSuccessInFlight_doesNotRecordAbandoned() {
        addActiveTabUploadedFromCache();
        expireUpload("token-cache");
        var watcher = newNoAbandonedOrFailedWatcher();

        mFuseboxAttachmentModelList.clear();

        watcher.assertExpected();
    }

    @Test
    public void testOnContextUploadStatusChanged_retryAfterSuccessFails_recordsNeitherOutcome() {
        addActiveTabUploadedFromCache();
        expireUpload("token-cache");
        var watcher = newNoAbandonedOrFailedWatcher();

        mFuseboxAttachmentModelList.onContextUploadStatusChanged(
                "token-fresh", ContextUploadStatus.UPLOAD_FAILED, ContextUploadErrorType.UNKNOWN);

        watcher.assertExpected();
        assertTrue(mFuseboxAttachmentModelList.isEmpty());
    }

    @Test
    public void testOnContextUploadStatusChanged_retryStarted_rebindsAttachment() {
        var attachment = addActiveTabUploadedFromCache();
        mFuseboxAttachmentModelList.addObserver(mListObserver);

        expireUpload("token-cache");

        assertFalse(attachment.isUploadComplete());
        verify(mListObserver).onItemRangeChanged(any(), eq(0), eq(1), eq(null));
    }

    @Test
    public void testClear_pendingSuggestedTab_doesNotRecordAbandoned() {
        mFuseboxAttachmentModelList.add(createSuggestedTabAttachment(/* tabId= */ 1, "token-sugg"));
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Omnibox.MobileFusebox.AttachmentAbandoned")
                        .build();

        mFuseboxAttachmentModelList.clear();

        watcher.assertExpected();
    }

    @Test
    public void testUploadFailedTerminal_suggestedTab_doesNotRecordAttachmentFailed() {
        mFuseboxAttachmentModelList.add(createSuggestedTabAttachment(/* tabId= */ 1, "token-sugg"));
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Omnibox.MobileFusebox.AttachmentFailed")
                        .build();

        mFuseboxAttachmentModelList.onContextUploadStatusChanged(
                "token-sugg",
                ContextUploadStatus.VALIDATION_FAILED,
                ContextUploadErrorType.BROWSER_PROCESSING_ERROR);
        watcher.assertExpected();
    }

    @Test
    public void testUploadSuccessful_suggestedTab_doesNotRecordAttachmentSucceeded() {
        mFuseboxAttachmentModelList.add(createSuggestedTabAttachment(/* tabId= */ 1, "token-sugg"));
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Omnibox.MobileFusebox.AttachmentSucceeded")
                        .build();

        notifyUploadSuccessful("token-sugg");
        watcher.assertExpected();
    }

    @Test
    public void testRemoveSuggestedTabs_uploadPending_doesNotRecordAbandoned() {
        mFuseboxAttachmentModelList.add(createSuggestedTabAttachment(/* tabId= */ 1, "token-sugg"));
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Omnibox.MobileFusebox.AttachmentAbandoned")
                        .build();

        mFuseboxAttachmentModelList.removeSuggestedTabs();
        watcher.assertExpected();
    }

    @Test
    public void testUploadStatusChanged_pendingSuggestedTab_doesNotBlockNotification() {
        addTabPickerAttachment(/* tabId= */ 1, "tab-token-1", /* loadDurationMs= */ 0L);
        mFuseboxAttachmentModelList.add(createSuggestedTabAttachment(/* tabId= */ 2, "token-sugg"));

        notifyUploadSuccessful("tab-token-1");

        verify(mListener).onAttachmentUploadStatusChanged();
    }

    @Test
    public void testUploadStatusChanged_retryInFlight_doesNotNotify() {
        addActiveTabUploadedFromCache();
        clearInvocations(mListener);

        expireUpload("token-cache");

        verify(mListener, never()).onAttachmentUploadStatusChanged();
    }
}
