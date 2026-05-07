// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ContentResolver;
import android.content.Context;
import android.database.MatrixCursor;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.provider.OpenableColumns;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;

import java.io.ByteArrayInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

/** Unit tests for {@link FuseboxAttachmentDetailsFetcher}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxAttachmentDetailsFetcherUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ContentResolver mContentResolver;
    @Mock private Callback<@Nullable FuseboxAttachment> mCallback;

    @Captor private ArgumentCaptor<FuseboxAttachment> mAttachmentCaptor;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    public void testFetchAttachmentDetails_forImageWithThumbnail()
            throws FileNotFoundException, IOException {
        Uri attachmentUri = Uri.parse("content://media/external/1");
        byte[] expectedData = new byte[] {1, 2, 3};
        String expectedTitle = "photo.png";
        String expectedMimeType = "image/png";

        when(mContentResolver.getType(attachmentUri)).thenReturn(expectedMimeType);
        when(mContentResolver.openInputStream(attachmentUri))
                .thenReturn(new ByteArrayInputStream(expectedData));

        MatrixCursor cursor =
                new MatrixCursor(new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE});
        cursor.addRow(new Object[] {expectedTitle, 1L});
        when(mContentResolver.query(eq(attachmentUri), isNull(), isNull(), isNull(), isNull()))
                .thenReturn(cursor);

        Bitmap thumbnail = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        when(mContentResolver.loadThumbnail(any(), any(), any())).thenReturn(thumbnail);

        FuseboxAttachmentDetailsFetcher fetcher =
                new FuseboxAttachmentDetailsFetcher(
                        mContext,
                        mContentResolver,
                        attachmentUri,
                        mCallback,
                        FuseboxAttachmentButtonType.GALLERY);

        fetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(mAttachmentCaptor.capture());
        FuseboxAttachment attachment = mAttachmentCaptor.getValue();

        assertNotNull(attachment);
        assertEquals(FuseboxAttachmentType.ATTACHMENT_IMAGE, attachment.type);
        assertEquals(expectedTitle, attachment.title);
        assertEquals(expectedMimeType, attachment.mimeType);
        assertArrayEquals(expectedData, attachment.data);
        assertEquals(FuseboxAttachmentButtonType.GALLERY, attachment.buttonType);
        assertNotNull(attachment.thumbnail);
        assertEquals(thumbnail, ((BitmapDrawable) attachment.thumbnail).getBitmap());
    }

    @Test
    public void testFetchAttachmentDetails_forTextFile() throws FileNotFoundException {
        Uri attachmentUri = Uri.parse("content://media/external/1");
        byte[] excpectedData = new byte[] {1, 2, 3};
        String expectedTitle = "file.txt";
        String expectedMimeType = "text/plain";

        when(mContentResolver.getType(attachmentUri)).thenReturn(expectedMimeType);
        when(mContentResolver.openInputStream(attachmentUri))
                .thenReturn(new ByteArrayInputStream(excpectedData));

        MatrixCursor cursor =
                new MatrixCursor(new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE});
        cursor.addRow(new Object[] {expectedTitle, 1L});
        when(mContentResolver.query(eq(attachmentUri), isNull(), isNull(), isNull(), isNull()))
                .thenReturn(cursor);

        FuseboxAttachmentDetailsFetcher fetcher =
                new FuseboxAttachmentDetailsFetcher(
                        mContext,
                        mContentResolver,
                        attachmentUri,
                        mCallback,
                        FuseboxAttachmentButtonType.FILES);

        fetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(mAttachmentCaptor.capture());
        FuseboxAttachment attachment = mAttachmentCaptor.getValue();

        assertNotNull(attachment);
        assertEquals(FuseboxAttachmentType.ATTACHMENT_FILE, attachment.type);
        assertEquals(expectedTitle, attachment.title);
        assertEquals(expectedMimeType, attachment.mimeType);
        assertArrayEquals(excpectedData, attachment.data);
        assertEquals(FuseboxAttachmentButtonType.FILES, attachment.buttonType);
    }

    @Test
    public void testFetchAttachmentDetails_forPdf() throws FileNotFoundException {
        Uri attachmentUri = Uri.parse("content://media/external/1");
        byte[] expectedData = new byte[] {1, 2, 3};
        String expectedTitle = "document.pdf";
        String expectedMimeType = "application/pdf";

        when(mContentResolver.getType(attachmentUri)).thenReturn(expectedMimeType);
        when(mContentResolver.openInputStream(attachmentUri))
                .thenReturn(new ByteArrayInputStream(expectedData));

        MatrixCursor cursor =
                new MatrixCursor(new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE});
        cursor.addRow(new Object[] {expectedTitle, 1L});
        when(mContentResolver.query(eq(attachmentUri), isNull(), isNull(), isNull(), isNull()))
                .thenReturn(cursor);

        FuseboxAttachmentDetailsFetcher fetcher =
                new FuseboxAttachmentDetailsFetcher(
                        mContext,
                        mContentResolver,
                        attachmentUri,
                        mCallback,
                        FuseboxAttachmentButtonType.FILES);

        fetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(mAttachmentCaptor.capture());
        FuseboxAttachment attachment = mAttachmentCaptor.getValue();

        assertNotNull(attachment);
        assertEquals(FuseboxAttachmentType.ATTACHMENT_PDF, attachment.type);
        assertEquals(expectedTitle, attachment.title);
        assertEquals(expectedMimeType, attachment.mimeType);
        assertArrayEquals(expectedData, attachment.data);
        assertEquals(FuseboxAttachmentButtonType.FILES, attachment.buttonType);
    }

    @Test
    public void testFetchAttachmentDetails_forImageNoThumbnail() throws Exception {
        Uri attachmentUri = Uri.parse("content://media/external/1");
        byte[] expectedData = new byte[] {1, 2, 3};
        String expectedTitle = "photo.png";
        String expectedMimeType = "image/png";

        when(mContentResolver.getType(attachmentUri)).thenReturn(expectedMimeType);
        when(mContentResolver.openInputStream(attachmentUri))
                .thenReturn(new ByteArrayInputStream(expectedData));

        MatrixCursor cursor =
                new MatrixCursor(new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE});
        cursor.addRow(new Object[] {expectedTitle, 1L});
        when(mContentResolver.query(eq(attachmentUri), isNull(), isNull(), isNull(), isNull()))
                .thenReturn(cursor);

        when(mContentResolver.loadThumbnail(any(), any(), any())).thenThrow(new IOException());

        FuseboxAttachmentDetailsFetcher fetcher =
                new FuseboxAttachmentDetailsFetcher(
                        mContext,
                        mContentResolver,
                        attachmentUri,
                        mCallback,
                        FuseboxAttachmentButtonType.GALLERY);

        fetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(mAttachmentCaptor.capture());
        FuseboxAttachment attachment = mAttachmentCaptor.getValue();

        assertNotNull(attachment);
        assertEquals(FuseboxAttachmentType.ATTACHMENT_IMAGE_NO_THUMBNAIL, attachment.type);
        assertEquals(expectedTitle, attachment.title);
        assertEquals(expectedMimeType, attachment.mimeType);
        assertArrayEquals(expectedData, attachment.data);
        assertEquals(FuseboxAttachmentButtonType.GALLERY, attachment.buttonType);
    }

    @Test
    public void testFetchAttachmentDetails_fileTooLarge() throws FileNotFoundException {
        Uri attachmentUri = Uri.parse("content://media/external/1");
        String expectedTitle = "large_file.txt";
        String expectedMimeType = "text/plain";

        when(mContentResolver.getType(attachmentUri)).thenReturn(expectedMimeType);

        MatrixCursor cursor =
                new MatrixCursor(new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE});
        cursor.addRow(
                new Object[] {
                    expectedTitle, FuseboxAttachmentDetailsFetcher.MAX_ATTACHMENT_SIZE_BYTES + 1
                });
        when(mContentResolver.query(eq(attachmentUri), isNull(), isNull(), isNull(), isNull()))
                .thenReturn(cursor);

        FuseboxAttachmentDetailsFetcher fetcher =
                new FuseboxAttachmentDetailsFetcher(
                        mContext,
                        mContentResolver,
                        attachmentUri,
                        mCallback,
                        FuseboxAttachmentButtonType.FILES);

        fetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(null);
    }
}
