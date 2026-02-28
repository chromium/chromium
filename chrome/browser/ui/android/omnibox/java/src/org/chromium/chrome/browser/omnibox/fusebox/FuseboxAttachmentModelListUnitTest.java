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
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.res.Resources;
import android.os.SystemClock;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.contextual_search.ContextUploadStatus;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;

import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;

@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxAttachmentModelListUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ComposeboxQueryControllerBridge mComposeboxQueryControllerBridge;
    @Mock private FuseboxAttachmentModelList.FuseboxAttachmentChangeListener mListener;

    private Resources mResources;
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
        OmniboxFeatures.sMultiattachmentFusebox.setForTesting(true);
        mFuseboxAttachmentModelList = new FuseboxAttachmentModelList();
        mFuseboxAttachmentModelList.setComposeboxQueryControllerBridge(
                mComposeboxQueryControllerBridge);
        verify(mComposeboxQueryControllerBridge).setFileUploadObserver(mFuseboxAttachmentModelList);
        mResources = ContextUtils.getApplicationContext().getResources();
        mFuseboxAttachmentModelList.addAttachmentChangeListener(mListener);
    }

    private FuseboxAttachment createTabAttachment(Tab tab) {
        return FuseboxAttachment.forTab(
                tab,
                /* bypassTabCache= */ false,
                mResources,
                FuseboxAttachmentButtonType.TAB_PICKER);
    }

    private FuseboxAttachment createTabAttachment(int tabId, String token) {
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(tabId);
        var attachment = createTabAttachment(tab);
        when(mComposeboxQueryControllerBridge.addTabContextFromCache(tabId)).thenReturn(token);
        return attachment;
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
        verify(mComposeboxQueryControllerBridge).setFileUploadObserver(null);

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
        verify(mComposeboxQueryControllerBridge).setFileUploadObserver(null);

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
        verify(mComposeboxQueryControllerBridge).setFileUploadObserver(null);

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

        mFuseboxAttachmentModelList.onFileUploadStatusChanged(
                "uploaded-token", ContextUploadStatus.UPLOAD_SUCCESSFUL);
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

        mFuseboxAttachmentModelList.onFileUploadStatusChanged(
                "pretokenized-token", ContextUploadStatus.UPLOAD_SUCCESSFUL);
        mFuseboxAttachmentModelList.onFileUploadStatusChanged(
                "uploaded-token", ContextUploadStatus.UPLOAD_FAILED);
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
        for (int i = 0; i < FuseboxAttachmentModelList.getMaxAttachments(); i++) {
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
        Tab tab = mock(Tab.class);
        WebContents webContents = mock(WebContents.class);
        RenderWidgetHostView renderWidgetHostView = mock(RenderWidgetHostView.class);
        doReturn(1).when(tab).getId();
        doReturn(true).when(tab).isInitialized();
        doReturn(false).when(tab).isFrozen();
        doReturn(webContents).when(tab).getWebContents();
        doReturn(renderWidgetHostView).when(webContents).getRenderWidgetHostView();
        when(mComposeboxQueryControllerBridge.addTabContext(tab)).thenReturn("token");

        FuseboxAttachment tabAttachment = createTabAttachment(tab);
        mFuseboxAttachmentModelList.add(tabAttachment);

        assertEquals("token", tabAttachment.getToken());
    }

    @Test
    public void testIncognitoTabDoesNotUseCache() {
        Tab tab = mock(Tab.class);
        WebContents webContents = mock(WebContents.class);
        RenderWidgetHostView renderWidgetHostView = mock(RenderWidgetHostView.class);
        doReturn(1).when(tab).getId();
        doReturn(true).when(tab).isInitialized();
        doReturn(false).when(tab).isFrozen();
        doReturn(true).when(tab).isIncognitoBranded();
        doReturn(webContents).when(tab).getWebContents();
        doReturn(renderWidgetHostView).when(webContents).getRenderWidgetHostView();
        when(mComposeboxQueryControllerBridge.addTabContext(tab)).thenReturn("token");

        FuseboxAttachment tabAttachment = createTabAttachment(tab);
        mFuseboxAttachmentModelList.add(tabAttachment);

        assertEquals("token", tabAttachment.getToken());
    }

    @Test
    public void testRetryTabUpload_failedImmediately() {
        Tab tab = mock(Tab.class);
        WebContents webContents = mock(WebContents.class);
        RenderWidgetHostView renderWidgetHostView = mock(RenderWidgetHostView.class);
        doReturn(1).when(tab).getId();
        doReturn(true).when(tab).isInitialized();
        doReturn(false).when(tab).isFrozen();
        doReturn(webContents).when(tab).getWebContents();
        doReturn(renderWidgetHostView).when(webContents).getRenderWidgetHostView();
        when(mComposeboxQueryControllerBridge.addTabContext(tab)).thenReturn("token2");
        when(mComposeboxQueryControllerBridge.addTabContextFromCache(1)).thenReturn("");

        FuseboxAttachment tabAttachment = createTabAttachment(tab);
        mFuseboxAttachmentModelList.add(tabAttachment);
        assertEquals("token2", tabAttachment.getToken());
    }

    @Test
    public void testRetryTabUpload_failedAfterTokenGenerated() {
        Tab tab = mock(Tab.class);
        WebContents webContents = mock(WebContents.class);
        RenderWidgetHostView renderWidgetHostView = mock(RenderWidgetHostView.class);
        doReturn(1).when(tab).getId();
        doReturn(true).when(tab).isInitialized();
        doReturn(false).when(tab).isFrozen();
        doReturn(webContents).when(tab).getWebContents();
        doReturn(renderWidgetHostView).when(webContents).getRenderWidgetHostView();
        when(mComposeboxQueryControllerBridge.addTabContextFromCache(1)).thenReturn("token");

        FuseboxAttachment tabAttachment = createTabAttachment(tab);
        mFuseboxAttachmentModelList.add(tabAttachment);
        assertEquals("token", tabAttachment.getToken());

        when(mComposeboxQueryControllerBridge.addTabContext(tab)).thenReturn("token2");
        mFuseboxAttachmentModelList.onFileUploadStatusChanged(
                "token", ContextUploadStatus.VALIDATION_FAILED);
        assertEquals("token2", tabAttachment.getToken());

        mFuseboxAttachmentModelList.onFileUploadStatusChanged(
                "token2", ContextUploadStatus.VALIDATION_FAILED);
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
        // Reset the listener to ignore the notification from the previous add.
        reset(mListener);

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
            mFuseboxAttachmentModelList.onFileUploadStatusChanged(
                    "token1", ContextUploadStatus.UPLOAD_SUCCESSFUL);
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
}
