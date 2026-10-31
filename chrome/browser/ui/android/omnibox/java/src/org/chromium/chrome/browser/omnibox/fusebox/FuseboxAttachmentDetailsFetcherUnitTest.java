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
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.times;
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
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentReadResult;
import org.chromium.chrome.browser.omnibox.fusebox.ImageFormatSniffer.ImageFormat;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.ui.base.MimeTypeUtils;

import java.io.ByteArrayInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/** Unit tests for {@link FuseboxAttachmentDetailsFetcher}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(OmniboxFeatureList.OMNIBOX_AIM_IMAGE_DOWNSCALING)
public class FuseboxAttachmentDetailsFetcherUnitTest {
    private static final int MAX_SQUARE_IMAGE_EDGE_SIZE =
            (int) Math.sqrt(FuseboxAttachmentDetailsFetcher.MAX_IMAGE_AREA);
    private static final long FILE_SIZE_SMALL = 1L;
    private static final String SAMPLE_TITLE = "file.txt";
    private static final String SAMPLE_PNG_TITLE = "photo.png";
    private static final int SAMPLE_IMAGE_DIMENSION = 512;

    /**
     * A payload with a valid PNG signature. Image fixtures must sniff as a Blink-decodable format,
     * otherwise the fetcher (correctly) routes them to the platform decoder for transcoding.
     */
    private static final byte[] SAMPLE_DATA = ImageFormatSnifferUnitTest.PNG;

    /** Plain-text bytes with no image signature, for non-image fixtures. */
    private static final byte[] SAMPLE_TEXT_DATA =
            "plain text attachment body".getBytes(StandardCharsets.US_ASCII);

    private static final Bitmap SAMPLE_SMALL_BITMAP =
            Bitmap.createBitmap(/* width= */ 1, /* height= */ 1, Bitmap.Config.ARGB_8888);
    private static final Uri SAMPLE_URI = Uri.parse("content://media/external/1");
    private static final Uri SAMPLE_URI_NO_FINAL_PATH_SEGMENT = Uri.parse("content://media");

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private ContentResolver mContentResolver;
    @Mock private Callback<@Nullable FuseboxAttachment> mCallback;

    @Captor private ArgumentCaptor<FuseboxAttachment> mAttachmentCaptor;

    private Context mContext;
    private FuseboxAttachmentDetailsFetcher mFetcher;
    private HistogramWatcher mOomHistogramWatcher;
    private byte[] mAttachmentData;

    @Before
    public void setUp() throws IOException {
        mContext = ApplicationProvider.getApplicationContext();
        setIsNetworkMetered(false);
        mAttachmentData = null;
        lenient()
                .when(mContentResolver.openInputStream(any()))
                .thenAnswer(
                        inv ->
                                mAttachmentData == null
                                        ? null
                                        : new ByteArrayInputStream(mAttachmentData));
        lenient()
                .when(mContentResolver.loadThumbnail(any(), any(), any()))
                .thenThrow(new IOException("No thumbnail"));
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
        mAttachmentData = data;
        if (thumbnail != null) {
            try {
                doReturn(thumbnail).when(mContentResolver).loadThumbnail(any(), any(), any());
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
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
                SAMPLE_TEXT_DATA,
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
        byte[] data = SAMPLE_DATA;
        Bitmap thumbnail = null;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.GALLERY;
        setupFetcherWithAttachment(title, mimeType, data, thumbnail, buttonType);
        // Make the fallback thumbnail generation fail.
        setupMockBitmapDecoder(/* bitmap= */ null, /* throwOom= */ false);

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
        byte[] data = SAMPLE_DATA;
        Bitmap thumbnail = null;
        long size = FuseboxAttachmentDetailsFetcher.MAX_ATTACHMENT_SIZE_BYTES + 1;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(title, mimeType, data, thumbnail, size, buttonType);
        setupMockBitmapDecoder(/* bitmap= */ null, /* throwOom= */ false);

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
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_AIM_IMAGE_DOWNSCALING)
    @EnableFeatures(ChromeFeatureList.LENS_BYPASS_COMPRESSION_FOR_C2PA)
    public void testFetchAttachmentDetails_downscalingDisabled_doesNotRecordC2paHistogram() {
        var c2paWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(FuseboxMetrics.ATTACHMENT_C2PA_DETECTED_HISTOGRAM)
                        .build();
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] c2paData = pngPayload("sample_prefix urn:c2pa: sample_suffix");
        Bitmap thumbnail = SAMPLE_SMALL_BITMAP;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(title, mimeType, c2paData, thumbnail, buttonType);
        setupMockImageDecoder(/* throwOom= */ false, /* throwIoException= */ true);
        FuseboxAttachmentDetailsFetcher.setFileStreamReaderForTesting((inputStream) -> c2paData);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                title,
                mimeType,
                c2paData,
                thumbnail,
                FuseboxAttachmentType.ATTACHMENT_IMAGE,
                buttonType);
        c2paWatcher.assertExpected();
    }

    @Test
    public void testFetchAttachmentDetails_imageLargeOnMeteredNetwork_passes() {
        setIsNetworkMetered(true);
        String title = "large_image.png";
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] data = SAMPLE_DATA;
        Bitmap thumbnail = null;
        long size =
                FuseboxAttachmentDetailsFetcher.MAX_ATTACHMENT_SIZE_BYTES_ON_METERED_NETWORK + 1;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(title, mimeType, data, thumbnail, size, buttonType);
        setupMockBitmapDecoder(/* bitmap= */ null, /* throwOom= */ false);

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
    public void testFetchAttachmentDetails_ioExceptionOnDownscaledImage_failsWithDecodeFailed() {
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] data = SAMPLE_DATA;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        // No thumbnail stub: the attachment is rejected before the thumbnail is ever fetched.
        setupFetcherWithAttachment(title, mimeType, data, /* thumbnail= */ null, buttonType);
        setupMockImageDecoder(/* throwOom= */ false, /* throwIoException= */ true);
        setupOomHistogramWatcher(/* oomExpected= */ false, MimeTypeUtils.Type.IMAGE);
        var readResultWatcher =
                newReadResultWatcher(
                        FuseboxAttachmentReadResult.DECODE_FAILED, MimeTypeUtils.Type.IMAGE);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        // The bytes were read in full but the platform decoder rejected them; native would reject
        // them too, so the attachment is dropped locally rather than forwarded.
        verify(mCallback).onResult(null);
        verifyNoMoreInteractions(mCallback);
        verifyOomHistogram();
        readResultWatcher.assertExpected();
    }

    @Test
    public void testFetchAttachmentDetails_emptyStream_failsWithEmpty() throws IOException {
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.GALLERY;
        setupFetcherWithAttachment(
                title, mimeType, /* data= */ new byte[] {}, /* thumbnail= */ null, buttonType);
        var readResultWatcher =
                newReadResultWatcher(FuseboxAttachmentReadResult.EMPTY, MimeTypeUtils.Type.IMAGE);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(null);
        verifyNoMoreInteractions(mCallback);
        // One initial open plus one retry.
        verify(mContentResolver, times(FuseboxAttachmentDetailsFetcher.MAX_READ_ATTEMPTS))
                .openInputStream(SAMPLE_URI);
        readResultWatcher.assertExpected();
    }

    @Test
    public void testFetchAttachmentDetails_shortReadRecoveredOnRetry_succeeds() throws IOException {
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] fullData = pngPayload("full payload bytes");
        byte[] truncatedData = Arrays.copyOf(fullData, fullData.length / 2);
        Bitmap thumbnail = SAMPLE_SMALL_BITMAP;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.GALLERY;
        setupFetcherWithAttachment(
                title, mimeType, fullData, thumbnail, (long) fullData.length, buttonType);
        // First open yields a truncated stream (e.g. cloud media still being prepared); the
        // second yields the full payload.
        when(mContentResolver.openInputStream(SAMPLE_URI))
                .thenReturn(new ByteArrayInputStream(truncatedData))
                .thenReturn(new ByteArrayInputStream(fullData));
        var readResultWatcher =
                newReadResultWatcher(
                        FuseboxAttachmentReadResult.SUCCESS_AFTER_RETRY, MimeTypeUtils.Type.IMAGE);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                title,
                mimeType,
                fullData,
                thumbnail,
                FuseboxAttachmentType.ATTACHMENT_IMAGE,
                buttonType);
        verify(mContentResolver, times(2)).openInputStream(SAMPLE_URI);
        readResultWatcher.assertExpected();
    }

    @Test
    public void testFetchAttachmentDetails_persistentShortRead_forwardsBytesAndRecords()
            throws IOException {
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] truncatedData = pngPayload("truncated");
        long advertisedSize = 1_000L;
        Bitmap thumbnail = SAMPLE_SMALL_BITMAP;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.GALLERY;
        setupFetcherWithAttachment(
                title, mimeType, truncatedData, thumbnail, advertisedSize, buttonType);
        var readResultWatcher =
                newReadResultWatcher(
                        FuseboxAttachmentReadResult.SHORT_READ, MimeTypeUtils.Type.IMAGE);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        // The provider-reported size is advisory; non-empty bytes are still forwarded, but the
        // short read is recorded so it can be correlated with downstream decode failures.
        verifyAttachmentResult(
                title,
                mimeType,
                truncatedData,
                thumbnail,
                FuseboxAttachmentType.ATTACHMENT_IMAGE,
                buttonType);
        verify(mContentResolver, times(FuseboxAttachmentDetailsFetcher.MAX_READ_ATTEMPTS))
                .openInputStream(SAMPLE_URI);
        readResultWatcher.assertExpected();
    }

    @Test
    public void testFetchAttachmentDetails_shortReadThenRetryThrows_keepsFirstRead()
            throws IOException {
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] truncatedData = pngPayload("truncated");
        long advertisedSize = 1_000L;
        Bitmap thumbnail = SAMPLE_SMALL_BITMAP;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.GALLERY;
        setupFetcherWithAttachment(
                title, mimeType, truncatedData, thumbnail, advertisedSize, buttonType);
        // One-shot pipe-backed provider: the first open yields a short stream, the second open
        // fails outright. The bytes from the first open must survive.
        when(mContentResolver.openInputStream(SAMPLE_URI))
                .thenReturn(new ByteArrayInputStream(truncatedData))
                .thenThrow(new FileNotFoundException("Cannot reopen pipe"));
        var readResultWatcher =
                newReadResultWatcher(
                        FuseboxAttachmentReadResult.SHORT_READ, MimeTypeUtils.Type.IMAGE);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                title,
                mimeType,
                truncatedData,
                thumbnail,
                FuseboxAttachmentType.ATTACHMENT_IMAGE,
                buttonType);
        readResultWatcher.assertExpected();
    }

    @Test
    public void testFetchAttachmentDetails_successfulRead_recordsSuccess() {
        setupFetcherWithTxtFileAttachment(FILE_SIZE_SMALL);
        var readResultWatcher =
                newReadResultWatcher(FuseboxAttachmentReadResult.SUCCESS, MimeTypeUtils.Type.TEXT);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(isNotNull());
        readResultWatcher.assertExpected();
    }

    @Test
    @EnableFeatures({
        OmniboxFeatureList.OMNIBOX_AIM_IMAGE_DOWNSCALING,
        ChromeFeatureList.LENS_BYPASS_COMPRESSION_FOR_C2PA
    })
    public void testFetchAttachmentDetails_downscalingWithC2paCheck_opensUriOnce()
            throws IOException {
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        Bitmap thumbnail = SAMPLE_SMALL_BITMAP;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.GALLERY;
        setupFetcherWithAttachment(title, mimeType, SAMPLE_DATA, thumbnail, buttonType);
        setupMockImageDecoder(/* throwOom= */ false, /* throwIoException= */ false);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(isNotNull());
        // Previously the C2PA sniff, the ImageDecoder source and the raw fallback each opened the
        // URI. Pipe-backed cloud media must only be streamed once.
        verify(mContentResolver).openInputStream(SAMPLE_URI);
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_AIM_IMAGE_DOWNSCALING)
    public void testFetchAttachmentDetails_heicByMimeType_transcodedToJpegWithoutFlag() {
        String title = "photo.heic";
        byte[] heicData = ImageFormatSnifferUnitTest.isobmff("heic");
        Bitmap thumbnail = SAMPLE_SMALL_BITMAP;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.GALLERY;
        setupFetcherWithAttachment(title, "image/heic", heicData, thumbnail, buttonType);
        setupMockImageDecoder(/* throwOom= */ false, /* throwIoException= */ false);
        var formatWatcher = newFormatWatcher(ImageFormat.HEIF, /* mismatch= */ false);
        var readResultWatcher =
                newReadResultWatcher(FuseboxAttachmentReadResult.SUCCESS, MimeTypeUtils.Type.IMAGE);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        // Blink cannot decode HEIF, so the payload must have been re-encoded as JPEG even though
        // the downscaling feature is disabled.
        FuseboxAttachment attachment = verifyTranscodedToJpeg(heicData);
        assertEquals(title, attachment.title);
        formatWatcher.assertExpected();
        readResultWatcher.assertExpected();
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_AIM_IMAGE_DOWNSCALING)
    public void testFetchAttachmentDetails_heicDeclaredAsJpeg_sniffedAndTranscoded() {
        byte[] heicData = ImageFormatSnifferUnitTest.isobmff("heix");
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.GALLERY;
        // Cloud-backed picker items may report image/jpeg for a HEIF payload.
        setupFetcherWithAttachment(
                "photo.jpg",
                MimeTypeUtils.IMAGE_JPEG_MIME_TYPE,
                heicData,
                SAMPLE_SMALL_BITMAP,
                buttonType);
        setupMockImageDecoder(/* throwOom= */ false, /* throwIoException= */ false);
        var formatWatcher = newFormatWatcher(ImageFormat.HEIF, /* mismatch= */ true);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyTranscodedToJpeg(heicData);
        formatWatcher.assertExpected();
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_AIM_IMAGE_DOWNSCALING)
    public void testFetchAttachmentDetails_dngDeclaredAsJpeg_sniffedAndTranscoded() {
        byte[] dngData = ImageFormatSnifferUnitTest.TIFF_LE;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.GALLERY;
        setupFetcherWithAttachment(
                "raw.dng",
                MimeTypeUtils.IMAGE_JPEG_MIME_TYPE,
                dngData,
                SAMPLE_SMALL_BITMAP,
                buttonType);
        setupMockImageDecoder(/* throwOom= */ false, /* throwIoException= */ false);
        var formatWatcher = newFormatWatcher(ImageFormat.TIFF, /* mismatch= */ true);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyTranscodedToJpeg(dngData);
        formatWatcher.assertExpected();
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_AIM_IMAGE_DOWNSCALING)
    public void testFetchAttachmentDetails_unknownSignatureWithImageMime_attemptsTranscode() {
        // e.g. SVG: declared as an image, but no raster signature. Blink would fail on it, so the
        // platform decoder gets a chance first; here it also fails, so we fail locally.
        byte[] svgData =
                "<svg xmlns=\"http://www.w3.org/2000/svg\"/>".getBytes(StandardCharsets.US_ASCII);
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(
                "icon.svg", "image/svg+xml", svgData, /* thumbnail= */ null, buttonType);
        setupMockImageDecoder(/* throwOom= */ false, /* throwIoException= */ true);
        var formatWatcher = newFormatWatcher(ImageFormat.UNKNOWN, /* mismatch= */ false);
        var readResultWatcher =
                newReadResultWatcher(
                        FuseboxAttachmentReadResult.DECODE_FAILED, MimeTypeUtils.Type.IMAGE);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(null);
        formatWatcher.assertExpected();
        readResultWatcher.assertExpected();
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_AIM_IMAGE_DOWNSCALING)
    public void testFetchAttachmentDetails_jpegDeclaredAsHeic_notTranscoded() {
        // The bytes are what matter: a real JPEG mislabelled as HEIC is forwarded untouched.
        byte[] jpegData = ImageFormatSnifferUnitTest.JPEG;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.GALLERY;
        setupFetcherWithAttachment(
                "photo.heic", "image/heic", jpegData, SAMPLE_SMALL_BITMAP, buttonType);
        // Would throw if the transcode path were taken.
        setupMockImageDecoder(/* throwOom= */ false, /* throwIoException= */ true);
        var formatWatcher = newFormatWatcher(ImageFormat.JPEG, /* mismatch= */ true);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                "photo.heic",
                "image/heic",
                jpegData,
                SAMPLE_SMALL_BITMAP,
                FuseboxAttachmentType.ATTACHMENT_IMAGE,
                buttonType);
        formatWatcher.assertExpected();
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_AIM_IMAGE_DOWNSCALING)
    public void testFetchAttachmentDetails_heicUndecodable_failsWithDecodeFailed() {
        byte[] heicData = ImageFormatSnifferUnitTest.isobmff("heic");
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.GALLERY;
        setupFetcherWithAttachment(
                "photo.heic", "image/heic", heicData, /* thumbnail= */ null, buttonType);
        setupMockImageDecoder(/* throwOom= */ false, /* throwIoException= */ true);
        var readResultWatcher =
                newReadResultWatcher(
                        FuseboxAttachmentReadResult.DECODE_FAILED, MimeTypeUtils.Type.IMAGE);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        // Raw HEIF bytes are never forwarded: native would fail on them anyway.
        verify(mCallback).onResult(null);
        readResultWatcher.assertExpected();
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_AIM_IMAGE_DOWNSCALING)
    public void testFetchAttachmentDetails_heicOomDuringTranscode_failsWithDecodeFailed() {
        byte[] heicData = ImageFormatSnifferUnitTest.isobmff("heic");
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.GALLERY;
        setupFetcherWithAttachment(
                "photo.heic", "image/heic", heicData, /* thumbnail= */ null, buttonType);
        setupMockImageDecoder(/* throwOom= */ true, /* throwIoException= */ false);
        setupOomHistogramWatcher(/* oomExpected= */ true, MimeTypeUtils.Type.IMAGE);
        var readResultWatcher =
                newReadResultWatcher(
                        FuseboxAttachmentReadResult.DECODE_FAILED, MimeTypeUtils.Type.IMAGE);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        // Unlike JPEG/PNG, there is no useful raw-bytes fallback for HEIF.
        verify(mCallback).onResult(null);
        verifyOomHistogram();
        readResultWatcher.assertExpected();
    }

    @Test
    @EnableFeatures({
        OmniboxFeatureList.OMNIBOX_AIM_IMAGE_DOWNSCALING,
        ChromeFeatureList.LENS_BYPASS_COMPRESSION_FOR_C2PA
    })
    public void testFetchAttachmentDetails_heicWithC2pa_stillTranscoded() {
        byte[] header = ImageFormatSnifferUnitTest.isobmff("heic");
        byte[] marker = " urn:c2pa: ".getBytes(StandardCharsets.US_ASCII);
        byte[] heicData = new byte[header.length + marker.length];
        System.arraycopy(header, 0, heicData, 0, header.length);
        System.arraycopy(marker, 0, heicData, header.length, marker.length);
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.GALLERY;
        setupFetcherWithAttachment(
                "photo.heic", "image/heic", heicData, SAMPLE_SMALL_BITMAP, buttonType);
        setupMockImageDecoder(/* throwOom= */ false, /* throwIoException= */ false);
        // The C2PA bypass forwards raw bytes, which is never viable for HEIF; the C2PA check is
        // skipped entirely so no C2PA histogram is recorded.
        var c2paWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(FuseboxMetrics.ATTACHMENT_C2PA_DETECTED_HISTOGRAM)
                        .build();

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyTranscodedToJpeg(heicData);
        c2paWatcher.assertExpected();
    }

    @Test
    public void testFetchAttachmentDetails_pngImage_recordsFormatWithoutMismatch() {
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.GALLERY;
        setupFetcherWithAttachment(
                SAMPLE_PNG_TITLE,
                MimeTypeUtils.IMAGE_PNG_MIME_TYPE,
                SAMPLE_DATA,
                SAMPLE_SMALL_BITMAP,
                buttonType);
        var formatWatcher = newFormatWatcher(ImageFormat.PNG, /* mismatch= */ false);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(isNotNull());
        formatWatcher.assertExpected();
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_AIM_IMAGE_DOWNSCALING)
    public void testFetchAttachmentDetails_heicWithGenericMimeType_recordedAndTranscoded() {
        byte[] heicData = ImageFormatSnifferUnitTest.isobmff("heic");
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        // A generic file picker may hand back application/octet-stream for a .heic file. That is
        // not an image mime type, but the payload is still HEIF and must be recorded/transcoded.
        setupFetcherWithAttachment(
                "IMG_0001.HEIC",
                "application/octet-stream",
                heicData,
                /* thumbnail= */ null,
                (long) heicData.length,
                buttonType);
        setupMockImageDecoder(/* throwOom= */ false, /* throwIoException= */ false);
        setupMockBitmapDecoder(/* bitmap= */ null, /* throwOom= */ false);
        // Format is recorded; the mismatch histogram is not, since no image mime was declared.
        var formatWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                FuseboxMetrics.ATTACHMENT_IMAGE_FORMAT_HISTOGRAM, ImageFormat.HEIF)
                        .expectNoRecords(FuseboxMetrics.ATTACHMENT_IMAGE_FORMAT_MISMATCH_HISTOGRAM)
                        .build();

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyTranscodedToJpeg(heicData);
        formatWatcher.assertExpected();
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_AIM_IMAGE_DOWNSCALING)
    public void testFetchAttachmentDetails_undecodableTiffWithGenericMimeType_keptAsPlainFile() {
        // Multi-page TIFFs and many camera RAW files sniff as TIFF, are rejected by Android's
        // ImageDecoder, and were not declared as images; keep uploading them as plain files.
        byte[] tiffData = ImageFormatSnifferUnitTest.TIFF_LE;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(
                "scan.tiff",
                "application/octet-stream",
                tiffData,
                /* thumbnail= */ null,
                (long) tiffData.length,
                buttonType);
        setupMockImageDecoder(/* throwOom= */ false, /* throwIoException= */ true);
        var readResultWatcher =
                newReadResultWatcher(
                        FuseboxAttachmentReadResult.SUCCESS, MimeTypeUtils.Type.UNKNOWN);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                "scan.tiff",
                "application/octet-stream",
                tiffData,
                /* expectedThumbnail= */ null,
                FuseboxAttachmentType.ATTACHMENT_FILE,
                buttonType);
        readResultWatcher.assertExpected();
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_AIM_IMAGE_DOWNSCALING)
    public void testFetchAttachmentDetails_oomTranscodingWithGenericMimeType_keptAsPlainFile() {
        byte[] tiffData = ImageFormatSnifferUnitTest.TIFF_LE;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(
                "scan.tiff",
                "application/octet-stream",
                tiffData,
                /* thumbnail= */ null,
                (long) tiffData.length,
                buttonType);
        setupMockImageDecoder(/* throwOom= */ true, /* throwIoException= */ false);
        setupOomHistogramWatcher(/* oomExpected= */ true, MimeTypeUtils.Type.UNKNOWN);
        var readResultWatcher =
                newReadResultWatcher(
                        FuseboxAttachmentReadResult.SUCCESS, MimeTypeUtils.Type.UNKNOWN);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                "scan.tiff",
                "application/octet-stream",
                tiffData,
                /* expectedThumbnail= */ null,
                FuseboxAttachmentType.ATTACHMENT_FILE,
                buttonType);
        verifyOomHistogram();
        readResultWatcher.assertExpected();
    }

    @Test
    public void testFetchAttachmentDetails_nonImageNonImageBytes_doesNotRecordFormat() {
        setupFetcherWithTxtFileAttachment(FILE_SIZE_SMALL);
        var formatWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(FuseboxMetrics.ATTACHMENT_IMAGE_FORMAT_HISTOGRAM)
                        .expectNoRecords(FuseboxMetrics.ATTACHMENT_IMAGE_FORMAT_MISMATCH_HISTOGRAM)
                        .build();

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(isNotNull());
        formatWatcher.assertExpected();
    }

    /**
     * Verifies the callback received a JPEG attachment whose bytes differ from {@code original}.
     */
    private FuseboxAttachment verifyTranscodedToJpeg(byte[] original) {
        verify(mCallback).onResult(mAttachmentCaptor.capture());
        FuseboxAttachment attachment = mAttachmentCaptor.getValue();
        assertNotNull(attachment);
        assertEquals(MimeTypeUtils.IMAGE_JPEG_MIME_TYPE, attachment.mimeType);
        assertThat(attachment.data).isNotEqualTo(original);
        return attachment;
    }

    private static HistogramWatcher newFormatWatcher(@ImageFormat int format, boolean mismatch) {
        return HistogramWatcher.newBuilder()
                .expectIntRecord(FuseboxMetrics.ATTACHMENT_IMAGE_FORMAT_HISTOGRAM, format)
                .expectBooleanRecord(
                        FuseboxMetrics.ATTACHMENT_IMAGE_FORMAT_MISMATCH_HISTOGRAM, mismatch)
                .build();
    }

    /** A PNG-signed payload followed by {@code ascii}, so that it sniffs as PNG. */
    private static byte[] pngPayload(String ascii) {
        byte[] sig = ImageFormatSnifferUnitTest.PNG;
        byte[] body = ascii.getBytes(StandardCharsets.US_ASCII);
        byte[] out = new byte[sig.length + body.length];
        System.arraycopy(sig, 0, out, 0, sig.length);
        System.arraycopy(body, 0, out, sig.length, body.length);
        return out;
    }

    private static HistogramWatcher newReadResultWatcher(
            @FuseboxAttachmentReadResult int expected, @MimeTypeUtils.Type int fileType) {
        return HistogramWatcher.newBuilder()
                .expectIntRecord(FuseboxMetrics.ATTACHMENT_READ_RESULT_HISTOGRAM, expected)
                .expectIntRecord(
                        FuseboxMetrics.getAttachmentReadResultHistogram(fileType), expected)
                .build();
    }

    @Test
    @EnableFeatures({
        OmniboxFeatureList.OMNIBOX_AIM_IMAGE_DOWNSCALING,
        ChromeFeatureList.LENS_BYPASS_COMPRESSION_FOR_C2PA
    })
    public void testFetchAttachmentDetails_c2paImage_bypassesDownscaling() {
        var c2paWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FuseboxMetrics.ATTACHMENT_C2PA_DETECTED_HISTOGRAM, true);
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] c2paData = pngPayload("sample_prefix urn:c2pa: sample_suffix");
        Bitmap thumbnail = SAMPLE_SMALL_BITMAP;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(title, mimeType, c2paData, thumbnail, buttonType);
        // If downscaling were triggered, this mock would throw an IOException, but C2PA should
        // bypass it.
        setupMockImageDecoder(/* throwOom= */ false, /* throwIoException= */ true);
        FuseboxAttachmentDetailsFetcher.setFileStreamReaderForTesting((inputStream) -> c2paData);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                title,
                mimeType,
                c2paData,
                thumbnail,
                FuseboxAttachmentType.ATTACHMENT_IMAGE,
                buttonType);
        c2paWatcher.assertExpected();
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_AIM_IMAGE_DOWNSCALING)
    @DisableFeatures(ChromeFeatureList.LENS_BYPASS_COMPRESSION_FOR_C2PA)
    public void testFetchAttachmentDetails_c2paImage_featureDisabled_downscalesImage() {
        var c2paWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FuseboxMetrics.ATTACHMENT_C2PA_DETECTED_HISTOGRAM, false);
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] c2paData = pngPayload("sample_prefix urn:c2pa: sample_suffix");
        Bitmap thumbnail = SAMPLE_SMALL_BITMAP;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(title, mimeType, c2paData, thumbnail, buttonType);
        // When flag is disabled, downscaling should be attempted. Mock decoder returning a
        // downscaled image.
        setupMockImageDecoder(/* throwOom= */ false, /* throwIoException= */ false);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback).onResult(mAttachmentCaptor.capture());
        FuseboxAttachment attachment = mAttachmentCaptor.getValue();

        assertNotNull(attachment);
        assertEquals(title, attachment.title);
        assertEquals(mimeType, attachment.mimeType);
        assertThat(attachment.data).isNotEqualTo(c2paData);
        assertEquals(thumbnail, getThumbnailFromAttachment(attachment));
        assertEquals(FuseboxAttachmentType.ATTACHMENT_IMAGE, attachment.type);
        assertEquals(buttonType, attachment.buttonType);
        c2paWatcher.assertExpected();
    }

    @Test
    @EnableFeatures({
        OmniboxFeatureList.OMNIBOX_AIM_IMAGE_DOWNSCALING,
        ChromeFeatureList.LENS_BYPASS_COMPRESSION_FOR_C2PA
    })
    public void testFetchAttachmentDetails_c2paImageAcrossChunkBoundary_bypassesDownscaling() {
        var c2paWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FuseboxMetrics.ATTACHMENT_C2PA_DETECTED_HISTOGRAM, true);
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        // Position "urn:c2pa:" across the 8192-byte chunk boundary (starts at byte 8190). The
        // payload still needs a PNG signature so that it sniffs as a Blink-decodable image.
        byte[] prefix = new byte[8190];
        System.arraycopy(
                ImageFormatSnifferUnitTest.PNG,
                0,
                prefix,
                0,
                ImageFormatSnifferUnitTest.PNG.length);
        byte[] marker = "urn:c2pa:".getBytes(StandardCharsets.US_ASCII);
        byte[] suffix = new byte[100];
        byte[] c2paData = new byte[prefix.length + marker.length + suffix.length];
        System.arraycopy(prefix, 0, c2paData, 0, prefix.length);
        System.arraycopy(marker, 0, c2paData, prefix.length, marker.length);
        System.arraycopy(suffix, 0, c2paData, prefix.length + marker.length, suffix.length);

        Bitmap thumbnail = SAMPLE_SMALL_BITMAP;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(title, mimeType, c2paData, thumbnail, buttonType);
        setupMockImageDecoder(/* throwOom= */ false, /* throwIoException= */ true);
        FuseboxAttachmentDetailsFetcher.setFileStreamReaderForTesting((inputStream) -> c2paData);

        mFetcher.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        RobolectricUtil.runAllBackgroundAndUi();

        verifyAttachmentResult(
                title,
                mimeType,
                c2paData,
                thumbnail,
                FuseboxAttachmentType.ATTACHMENT_IMAGE,
                buttonType);
        c2paWatcher.assertExpected();
    }

    @Test
    public void testFetchAttachmentDetails_imageOomOnReadStream_fails() {
        String title = SAMPLE_PNG_TITLE;
        String mimeType = MimeTypeUtils.IMAGE_PNG_MIME_TYPE;
        byte[] data = SAMPLE_DATA;
        @FuseboxAttachmentButtonType int buttonType = FuseboxAttachmentButtonType.FILES;
        setupFetcherWithAttachment(title, mimeType, data, /* thumbnail= */ null, buttonType);
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
        setupFetcherWithAttachment(title, mimeType, data, /* thumbnail= */ null, buttonType);
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
