// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNotNull;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.ContentResolver;
import android.content.Context;
import android.database.MatrixCursor;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.util.Size;

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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
import org.chromium.ui.base.MimeTypeUtils;

import java.io.ByteArrayInputStream;
import java.io.IOException;

/** Unit tests for {@link FuseboxAttachmentDetailsFetcher}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxAttachmentDetailsFetcherUnitTest {
    private static final int MAX_SQUARE_IMAGE_EDGE_SIZE =
            (int) Math.sqrt(FuseboxAttachmentDetailsFetcher.MAX_IMAGE_AREA);
    private static final long FILE_SIZE_SMALL = 1L;
    private static final String SAMPLE_TITLE = "file.txt";
    private static final String SAMPLE_PNG_TITLE = "photo.png";
    private static final int SAMPLE_IMAGE_DIMENSION = 512;
    private static final byte[] SAMPLE_DATA = new byte[] {1, 2, 3};
    private static final Bitmap SAMPLE_SMALL_BITMAP =
            Bitmap.createBitmap(/* width= */ 1, /* height= */ 1, Bitmap.Config.ARGB_8888);
    private static final Uri SAMPLE_URI = Uri.parse("content://media/external/1");
    private static final Uri SAMPLE_URI_NO_FINAL_PATH_SEGMENT = Uri.parse("content://media");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ContentResolver mContentResolver;
    @Mock private Callback<@Nullable FuseboxAttachment> mCallback;

    @Captor private ArgumentCaptor<FuseboxAttachment> mAttachmentCaptor;

    private Context mContext;
    private FuseboxAttachmentDetailsFetcher mFetcher;
    private HistogramWatcher mOomHistogramWatcher;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        setIsNetworkMetered(false);
    }

    private void setIsNetworkMetered(boolean isMetered) {
        DeviceConditions.setForTesting(
                new DeviceConditions(
                        /* powerConnected= */ true,
                        /* batteryPercentage= */ 100,
                        /* netConnectionType= */ 0,
                        /* powerSaveOn= */ false,
                        isMetered,
                        /* screenOnAndUnlocked= */ true));
    }

    private void setupOomHistogramWatcher(
            @Nullable Boolean oomExpected, @MimeTypeUtils.Type int fileType) {
        if (oomExpected == null) {
            mOomHistogramWatcher =
                    HistogramWatcher.newBuilder()
                            .expectNoRecords(FuseboxMetrics.ATTACHMENT_LOAD_OOM_HISTOGRAM)
                            .expectNoRecords(FuseboxMetrics.getAttachmentLoadOomHistogram(fileType))
                            .build();
            return;
        }
        mOomHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                FuseboxMetrics.ATTACHMENT_LOAD_OOM_HISTOGRAM, oomExpected)
                        .expectBooleanRecord(
                                FuseboxMetrics.getAttachmentLoadOomHistogram(fileType), oomExpected)
                        .build();
    }

    private void verifyOomHistogram() {
        mOomHistogramWatcher.assertExpected();
    }

    /**
     * Mock the mContentResolver with the given attachment details, and initialize the mFetcher to
     * load those arguments into an attachment.
     */
    private void setupFetcherWithAttachment(
            Uri attachmentUri,
            @Nullable String title,
            @Nullable String mimeType,
            byte @Nullable [] data,
            @Nullable Bitmap thumbnail,
            @Nullable Long sizeBytes,
            @FuseboxAttachmentButtonType int buttonType) {
        MatrixCursor cursor =
                new MatrixCursor(new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE});
        cursor.addRow(new Object[] {title, sizeBytes});

        when(mContentResolver.getType(attachmentUri)).thenReturn(mimeType);
        try {
            lenient()
                    .when(mContentResolver.openInputStream(attachmentUri))
                    .thenReturn(data == null ? null : new ByteArrayInputStream(data));
            if (thumbnail != null) {
                when(mContentResolver.loadThumbnail(any(), any(), any())).thenReturn(thumbnail);
            } else {
                when(mContentResolver.loadThumbnail(any(), any(), any()))
                        .thenThrow(new IOException("No thumbnail"));
            }
        } catch (IOException e) {
            // The compiler requires us to catch the exceptions thrown by the mocked methods, but
            // we know they aren't actually called here, and so no exception should be thrown.
            throw new RuntimeException(e);
        }
        when(mContentResolver.query(eq(attachmentUri), isNull(), isNull(), isNull(), isNull()))
                .thenReturn(cursor);

        mFetcher =
                new FuseboxAttachmentDetailsFetcher(
                        mContext, mContentResolver, attachmentUri, mCallback, buttonType);
    }

    private void setupFetcherWithAttachment(
            @Nullable String title,
            @Nullable String mimeType,
            byte @Nullable [] data,
            @Nullable Bitmap thumbnail,
            @Nullable Long sizeBytes,
            @FuseboxAttachmentButtonType int buttonType) {
        setupFetcherWithAttachment(
                SAMPLE_URI, title, mimeType, data, thumbnail, sizeBytes, buttonType);
    }

    private void setupFetcherWithAttachment(
            @Nullable String title,
            @Nullable String mimeType,
            byte @Nullable [] data,
            @Nullable Bitmap thumbnail,
            @FuseboxAttachmentButtonType int buttonType) {
        setupFetcherWithAttachment(
                SAMPLE_URI, title, mimeType, data, thumbnail, FILE_SIZE_SMALL, buttonType);
    }

    private void setupFetcherWithTxtFileAttachment(long sizeBytes) {
        setupFetcherWithAttachment(
                /* title= */ "large_file.txt",
                MimeTypeUtils.TEXT_PLAIN_MIME_TYPE,
                SAMPLE_DATA,
                /* thumbnail= */ null,
                sizeBytes,
                FuseboxAttachmentButtonType.FILES);
    }

    private static @Nullable Bitmap getThumbnailFromAttachment(FuseboxAttachment attachment) {
        return attachment.thumbnail == null
                ? null
                : ((BitmapDrawable) attachment.thumbnail).getBitmap();
    }

    private static void setupMockBitmapDecoder(@Nullable Bitmap bitmap, boolean throwOom) {
        FuseboxAttachmentDetailsFetcher.setBitmapDecoderForTesting(
                (array, offset, length, options) -> {
                    if (options != null && options.inJustDecodeBounds) {
                        options.outWidth = SAMPLE_IMAGE_DIMENSION;
                        options.outHeight = SAMPLE_IMAGE_DIMENSION;
                        return null;
                    }
                    if (throwOom) {
                        throw new OutOfMemoryError("Triggered OOM for testing");
                    }
                    return bitmap;
                });
    }

    private static void setupMockFileStreamReader(boolean throwOom, boolean throwIoException) {
        FuseboxAttachmentDetailsFetcher.setFileStreamReaderForTesting(
                (inputStream) -> {
                    if (throwOom) {
                        throw new OutOfMemoryError("Triggered OOM for testing");
                    }
                    if (throwIoException) {
                        throw new IOException("Triggered IOException for testing");
                    }
                    return SAMPLE_DATA;
                });
    }

    private static void setupMockImageDecoder(boolean throwOom, boolean throwIoException) {
        FuseboxAttachmentDetailsFetcher.setImageDecoderForTesting(
                (source, listener) -> {
                    if (throwOom) {
                        throw new OutOfMemoryError("Triggered OOM for testing");
                    }
                    if (throwIoException) {
                        throw new IOException("Triggered IOException for testing");
                    }
                    return SAMPLE_SMALL_BITMAP;
                });
    }

    private void verifyAttachmentResult(
            String expectedTitle,
            String expectedMimeType,
            byte[] expectedData,
            @Nullable Bitmap expectedThumbnail,
            @FuseboxAttachmentType int expectedType,
            @FuseboxAttachmentButtonType int expectedButtonType) {
        verify(mCallback).onResult(mAttachmentCaptor.capture());
        verifyNoMoreInteractions(mCallback);
        FuseboxAttachment attachment = mAttachmentCaptor.getValue();

        assertNotNull(attachment);
        assertEquals(expectedTitle, attachment.title);
        assertEquals(expectedMimeType, attachment.mimeType);
        assertArrayEquals(expectedData, attachment.data);
        assertEquals(expectedThumbnail, getThumbnailFromAttachment(attachment));
        assertEquals(expectedType, attachment.type);
        assertEquals(expectedButtonType, attachment.buttonType);
    }

    private static void verifyDownscaleTargetWithinLimits(@Nullable Size target, String message) {
        assertWithMessage(message).that(target).isNotNull();

        long targetArea = (long) target.getWidth() * target.getHeight();
        long maxArea = FuseboxAttachmentDetailsFetcher.MAX_IMAGE_AREA;

        assertWithMessage(message).that(targetArea).isAtMost(maxArea);
    }

    @Test
    public void testRegularLimitAtLeastAsLargeAsMeteredLimit() {
        assertThat(FuseboxAttachmentDetailsFetcher.MAX_ATTACHMENT_SIZE_BYTES)
                .isAtLeast(
                        FuseboxAttachmentDetailsFetcher
                                .MAX_ATTACHMENT_SIZE_BYTES_ON_METERED_NETWORK);
    }

    @Test
    public void testFetchAttachmentDetails_smallFileSizePasses() {
        setupFetcherWithTxtFileAttachment(FILE_SIZE_SMALL);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(isNotNull());
        verifyNoMoreInteractions(mCallback);
    }

    @Test
    public void testFetchAttachmentDetails_fileTooLargeOnUnmeteredNetwork_fails() {
        setIsNetworkMetered(false);
        long fileSizeTooLarge = FuseboxAttachmentDetailsFetcher.MAX_ATTACHMENT_SIZE_BYTES + 1;
        setupFetcherWithTxtFileAttachment(fileSizeTooLarge);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(null);
        verifyNoMoreInteractions(mCallback);
    }

    @Test
    public void testFetchAttachmentDetails_fileTooLargeOnMeteredNetwork_fails() {
        setIsNetworkMetered(true);
        long fileSizeTooLarge =
                FuseboxAttachmentDetailsFetcher.MAX_ATTACHMENT_SIZE_BYTES_ON_METERED_NETWORK + 1;
        setupFetcherWithTxtFileAttachment(fileSizeTooLarge);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(null);
        verifyNoMoreInteractions(mCallback);
    }

    @Test
    public void testFetchAttachmentDetails_maxSizeAllowedOnUnmeteredNetwork() {
        setIsNetworkMetered(false);
        setupFetcherWithTxtFileAttachment(
                FuseboxAttachmentDetailsFetcher.MAX_ATTACHMENT_SIZE_BYTES);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(isNotNull());
        verifyNoMoreInteractions(mCallback);
    }

    @Test
    public void testFetchAttachmentDetails_forImageWithThumbnail() {
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] data = SAMPLE_DATA;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(title, mimeType, data, SAMPLE_SMALL_BITMAP, buttonType);
        setupOomHistogramWatcher(/* oomExpected= */ false, MimeTypeUtils.Type.IMAGE);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                title,
                mimeType,
                data,
                SAMPLE_SMALL_BITMAP,
                FuseboxAttachmentType.ATTACHMENT_IMAGE,
                buttonType);
        verifyOomHistogram();
    }

    @Test
    public void testFetchAttachmentDetails_forTextFile() {
        String title = "file.txt";
        String mimeType = MimeTypeUtils.TEXT_PLAIN_MIME_TYPE;
        byte[] data = SAMPLE_DATA;
        Bitmap thumbnail = null;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(title, mimeType, data, thumbnail, buttonType);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                title,
                mimeType,
                data,
                thumbnail,
                FuseboxAttachmentType.ATTACHMENT_FILE,
                buttonType);
    }

    @Test
    public void testFetchAttachmentDetails_forPdf() {
        String title = "document.pdf";
        String mimeType = MimeTypeUtils.PDF_MIME_TYPE;
        byte[] data = SAMPLE_DATA;
        Bitmap thumbnail = null;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(title, mimeType, data, thumbnail, buttonType);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                title, mimeType, data, thumbnail, FuseboxAttachmentType.ATTACHMENT_PDF, buttonType);
    }

    @Test
    public void testFetchAttachmentDetails_forImageNoThumbnail() {
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        // We pass an empty byte array so that the fallback thumbnail generation fails.
        byte[] data = new byte[] {};
        Bitmap thumbnail = null;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.GALLERY;
        setupFetcherWithAttachment(title, mimeType, data, thumbnail, buttonType);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                title,
                mimeType,
                data,
                thumbnail,
                FuseboxAttachmentType.ATTACHMENT_IMAGE_NO_THUMBNAIL,
                buttonType);
    }

    @Test
    public void testFetchAttachmentDetails_imageLargeOnUnmeteredNetwork_passes() {
        setIsNetworkMetered(false);
        String title = "large_image.png";
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] data = new byte[] {};
        Bitmap thumbnail = null;
        long size = FuseboxAttachmentDetailsFetcher.MAX_ATTACHMENT_SIZE_BYTES + 1;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(title, mimeType, data, thumbnail, size, buttonType);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                title,
                mimeType,
                data,
                thumbnail,
                FuseboxAttachmentType.ATTACHMENT_IMAGE_NO_THUMBNAIL,
                buttonType);
    }

    @Test
    public void testFetchAttachmentDetails_imageLargeOnMeteredNetwork_passes() {
        setIsNetworkMetered(true);
        String title = "large_image.png";
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] data = new byte[] {};
        Bitmap thumbnail = null;
        long size =
                FuseboxAttachmentDetailsFetcher.MAX_ATTACHMENT_SIZE_BYTES_ON_METERED_NETWORK + 1;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(title, mimeType, data, thumbnail, size, buttonType);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                title,
                mimeType,
                data,
                thumbnail,
                FuseboxAttachmentType.ATTACHMENT_IMAGE_NO_THUMBNAIL,
                buttonType);
    }

    @Test
    public void testFetchAttachmentDetails_nullMimeType_fails() {
        setupFetcherWithAttachment(
                SAMPLE_TITLE,
                /* mimeType= */ null,
                SAMPLE_DATA,
                /* thumbnail= */ null,
                FuseboxAttachmentButtonType.FILES);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(null);
        verifyNoMoreInteractions(mCallback);
    }

    @Test
    public void testFetchAttachmentDetails_nullTitle_fails() {
        setupFetcherWithAttachment(
                SAMPLE_URI_NO_FINAL_PATH_SEGMENT,
                /* title= */ null,
                MimeTypeUtils.TEXT_PLAIN_MIME_TYPE,
                SAMPLE_DATA,
                /* thumbnail= */ null,
                FILE_SIZE_SMALL,
                FuseboxAttachmentButtonType.FILES);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(null);
        verifyNoMoreInteractions(mCallback);
    }

    @Test
    public void testFetchAttachmentDetails_nullSizeForNonImage_fails() {
        setupFetcherWithAttachment(
                SAMPLE_TITLE,
                MimeTypeUtils.TEXT_PLAIN_MIME_TYPE,
                SAMPLE_DATA,
                /* thumbnail= */ null,
                /* sizeBytes= */ null,
                FuseboxAttachmentButtonType.FILES);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(null);
        verifyNoMoreInteractions(mCallback);
    }

    @Test
    public void testFetchAttachmentDetails_nullData_fails() {
        setupFetcherWithAttachment(
                SAMPLE_TITLE,
                MimeTypeUtils.TEXT_PLAIN_MIME_TYPE,
                /* data= */ null,
                /* thumbnail= */ null,
                FuseboxAttachmentButtonType.FILES);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(null);
        verifyNoMoreInteractions(mCallback);
    }

    @Test
    public void testFetchAttachmentDetails_thumbnailGenRequired_passesWithThumbnail() {
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] data = SAMPLE_DATA;
        Bitmap thumbnail = SAMPLE_SMALL_BITMAP;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.GALLERY;
        setupFetcherWithAttachment(title, mimeType, data, /* thumbnail= */ null, buttonType);
        setupMockBitmapDecoder(thumbnail, /* throwOom= */ false);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                title,
                mimeType,
                data,
                thumbnail,
                FuseboxAttachmentType.ATTACHMENT_IMAGE,
                buttonType);
    }

    @Test
    public void testFetchAttachmentDetails_oomOnThumbnailGen_passesWithoutThumbnail() {
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] data = SAMPLE_DATA;
        Bitmap thumbnail = null;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.GALLERY;
        setupFetcherWithAttachment(title, mimeType, data, thumbnail, buttonType);
        setupMockBitmapDecoder(thumbnail, /* throwOom= */ true);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                title,
                mimeType,
                data,
                /* expectedThumbnail= */ null,
                FuseboxAttachmentType.ATTACHMENT_IMAGE_NO_THUMBNAIL,
                buttonType);
    }

    @Test
    public void testFetchAttachmentDetails_oomOnReadStream_fails() {
        setupFetcherWithTxtFileAttachment(FILE_SIZE_SMALL);
        setupMockFileStreamReader(/* throwOom= */ true, /* throwIoException= */ false);
        setupOomHistogramWatcher(/* oomExpected= */ true, MimeTypeUtils.Type.TEXT);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(null);
        verifyNoMoreInteractions(mCallback);
        verifyOomHistogram();
    }

    @Test
    public void testFetchAttachmentDetails_ioExceptionOnReadStream_fails() {
        setupFetcherWithTxtFileAttachment(FILE_SIZE_SMALL);
        setupMockFileStreamReader(/* throwOom= */ false, /* throwIoException= */ true);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(null);
        verifyNoMoreInteractions(mCallback);
    }

    @Test
    @EnableFeatures("OmniboxAimImageDownscaling")
    public void testFetchAttachmentDetails_oomOnDownscaledImage_fallsBackToRegular() {
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] data = SAMPLE_DATA;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(title, mimeType, data, SAMPLE_SMALL_BITMAP, buttonType);
        setupMockImageDecoder(/* throwOom= */ true, /* throwIoException= */ false);
        setupOomHistogramWatcher(/* oomExpected= */ true, MimeTypeUtils.Type.IMAGE);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                title,
                mimeType,
                data,
                SAMPLE_SMALL_BITMAP,
                FuseboxAttachmentType.ATTACHMENT_IMAGE,
                buttonType);
        verifyOomHistogram();
    }

    @Test
    @EnableFeatures("OmniboxAimImageDownscaling")
    public void testFetchAttachmentDetails_ioExceptionOnDownscaledImage_fallsBackToRegular() {
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] data = SAMPLE_DATA;
        Bitmap thumbnail = SAMPLE_SMALL_BITMAP;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(title, mimeType, data, thumbnail, buttonType);
        setupMockImageDecoder(/* throwOom= */ false, /* throwIoException= */ true);
        setupOomHistogramWatcher(/* oomExpected= */ false, MimeTypeUtils.Type.IMAGE);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                title,
                mimeType,
                data,
                thumbnail,
                FuseboxAttachmentType.ATTACHMENT_IMAGE,
                buttonType);
        verifyOomHistogram();
    }

    @Test
    public void testFetchAttachmentDetails_imageOomOnReadStream_fails() {
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] data = SAMPLE_DATA;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(title, mimeType, data, SAMPLE_SMALL_BITMAP, buttonType);
        setupMockFileStreamReader(/* throwOom= */ true, /* throwIoException= */ false);
        setupOomHistogramWatcher(/* oomExpected= */ true, MimeTypeUtils.Type.IMAGE);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(null);
        verifyNoMoreInteractions(mCallback);
        verifyOomHistogram();
    }

    @Test
    public void testFetchAttachmentDetails_imageIoExceptionOnReadStream_fails() {
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] data = SAMPLE_DATA;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(title, mimeType, data, SAMPLE_SMALL_BITMAP, buttonType);
        setupMockFileStreamReader(/* throwOom= */ false, /* throwIoException= */ true);
        setupOomHistogramWatcher(/* oomExpected= */ false, MimeTypeUtils.Type.IMAGE);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(null);
        verifyNoMoreInteractions(mCallback);
        verifyOomHistogram();
    }

    @Test
    public void testGetTargetSizeForDownscaling_areaUnder_edgesUnder_notScaled() {
        int width = MAX_SQUARE_IMAGE_EDGE_SIZE;
        int height = MAX_SQUARE_IMAGE_EDGE_SIZE / 2;

        Size targetSize =
                FuseboxAttachmentDetailsFetcher.getTargetSizeForDownscaling(width, height);

        assertWithMessage("Area under limit, image should not be scaled").that(targetSize).isNull();
    }

    @Test
    public void testGetTargetSizeForDownscaling_areaUnder_oneEdgeOver_notScaled() {
        int width = MAX_SQUARE_IMAGE_EDGE_SIZE * 2;
        int height = MAX_SQUARE_IMAGE_EDGE_SIZE / 2 - 1;

        Size targetSize =
                FuseboxAttachmentDetailsFetcher.getTargetSizeForDownscaling(width, height);

        assertWithMessage("Area under limit, image should not be scaled").that(targetSize).isNull();
    }

    @Test
    public void testGetTargetSizeForDownscaling_areaOver_edgesOver_scaled() {
        int width = MAX_SQUARE_IMAGE_EDGE_SIZE * 2;
        int height = MAX_SQUARE_IMAGE_EDGE_SIZE * 2;

        Size targetSize =
                FuseboxAttachmentDetailsFetcher.getTargetSizeForDownscaling(width, height);

        verifyDownscaleTargetWithinLimits(targetSize, "Area over limit, image should be scaled");
    }

    @Test
    public void testGetTargetSizeForDownscaling_areaOver_oneEdgeOver_scaled() {
        int width = MAX_SQUARE_IMAGE_EDGE_SIZE * 3;
        int height = MAX_SQUARE_IMAGE_EDGE_SIZE / 2;

        Size targetSize =
                FuseboxAttachmentDetailsFetcher.getTargetSizeForDownscaling(width, height);

        verifyDownscaleTargetWithinLimits(targetSize, "Area over limit, image should be scaled");
    }
}
