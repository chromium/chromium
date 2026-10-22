// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.Bitmap.CompressFormat;
import android.graphics.BitmapFactory;
import android.graphics.ImageDecoder;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.SystemClock;
import android.provider.OpenableColumns;
import android.text.TextUtils;
import android.util.Size;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.AsyncTask;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentReadResult;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentSizeLimitCheck;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.MimeTypeUtils;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;

/**
 * An AsyncTask that fetches attachment details (thumbnail, title, and description) from a content
 * URI.
 *
 * <p>Note: we're using Optional, because AsyncTask is explicitly @NonNull - and we need a way to
 * Handle cases where we cannot access the content.
 */
@NullMarked
class FuseboxAttachmentDetailsFetcher extends AsyncTask<Boolean> {
    private static final String TAG = "FbAttachFetcher";
    private static final int THUMBNAIL_BITMAP_EDGE_SIZE = 256;
    @VisibleForTesting static final int MAX_IMAGE_AREA = 1600 * 1600;

    @VisibleForTesting
    static final long MAX_ATTACHMENT_SIZE_BYTES = 100 * 1000 * 1000L; /* 100 MB */

    @VisibleForTesting
    static final long MAX_ATTACHMENT_SIZE_BYTES_ON_METERED_NETWORK = 20 * 1000 * 1000L; /* 20 MB */

    /**
     * Number of times to open the content URI before giving up on an empty or short read. Cloud
     * media providers may hand back a partially-populated stream on first open; a single retry is
     * cheap and usually sufficient once the provider has cached the item.
     */
    @VisibleForTesting static final int MAX_READ_ATTEMPTS = 2;

    private static BitmapDecoder sBitmapDecoder = BitmapFactory::decodeByteArray;
    private static FileStreamReader sFileStreamReader = FileUtils::readStream;
    private static DownscaledImageDecoder sImageDecoder = ImageDecoder::decodeBitmap;

    private final Context mContext;
    private final ContentResolver mContentResolver;
    private final Uri mUri;
    private final Callback<@Nullable FuseboxAttachment> mCallback;
    private final long mStartTime = SystemClock.elapsedRealtime();
    private final @FuseboxAttachmentButtonType int mButtonType;
    private @Nullable Drawable mThumbnail;
    private @Nullable String mTitle;
    private @Nullable String mMimeType;
    private byte @Nullable [] mData;

    FuseboxAttachmentDetailsFetcher(
            Context context,
            ContentResolver contentResolver,
            Uri uri,
            Callback<@Nullable FuseboxAttachment> callback,
            @FuseboxAttachmentButtonType int buttonType) {
        mContext = context;
        mContentResolver = contentResolver;
        mUri = uri;
        mCallback = callback;
        mButtonType = buttonType;
    }

    @Override
    protected Boolean doInBackground() {
        Long size;
        try (Cursor cursor =
                mContentResolver.query(
                        mUri,
                        /* projection= */ null,
                        /* selection= */ null,
                        /* selectionArgs= */ null,
                        /* sortOrder= */ null)) {
            mTitle = fetchTitle(cursor);
            size = fetchSize(cursor);
        }

        mMimeType = fetchMimeType();

        // Bail: don't add the item if we miss metadata.
        if (TextUtils.isEmpty(mTitle) || TextUtils.isEmpty(mMimeType)) return false;

        if (size != null) {
            recordAttachmentSize(size, mMimeType);
        }

        boolean isMetered = DeviceConditions.isCurrentActiveNetworkMetered(mContext);
        boolean isImage = MimeTypeUtils.getTypeFromMimeType(mMimeType) == MimeTypeUtils.Type.IMAGE;

        /* Only exempt images from size limits, as they should be downscaled */
        if (!isImage && (size == null || size > getMaxSizeLimit(isMetered))) {
            if (size == null) return false;
            recordAttachmentSizeLimitCheck(isMetered, /* isTooLarge= */ true);
            return false;
        }

        recordAttachmentSizeLimitCheck(isMetered, /* isTooLarge= */ false);

        mData = fetchData(mMimeType, size);
        if (mData == null) return false;

        mThumbnail = fetchThumbnail(mData, mMimeType);

        return true;
    }

    @Override
    protected void onPostExecute(Boolean result) {
        if (mMimeType == null || mTitle == null || mData == null || result == null || !result) {
            mCallback.onResult(/* result= */ null);
            return;
        }

        FuseboxAttachment attachment =
                switch (MimeTypeUtils.getTypeFromMimeType(mMimeType)) {
                    case MimeTypeUtils.Type.IMAGE ->
                            mThumbnail != null
                                    ? FuseboxAttachment.forImage(
                                            mThumbnail,
                                            mTitle,
                                            mMimeType,
                                            mData,
                                            mStartTime,
                                            mButtonType)
                                    : FuseboxAttachment.forImageNoThumbnail(
                                            mTitle, mMimeType, mData, mStartTime, mButtonType);
                    case MimeTypeUtils.Type.PDF ->
                            FuseboxAttachment.forPdf(
                                    mThumbnail, mTitle, mMimeType, mData, mStartTime, mButtonType);
                    default ->
                            FuseboxAttachment.forFile(
                                    mThumbnail, mTitle, mMimeType, mData, mStartTime, mButtonType);
                };

        mCallback.onResult(attachment);
    }

    private @Nullable String fetchTitle(@Nullable Cursor cursor) {
        String fallbackTitle = mUri.getLastPathSegment();
        if (cursor == null) {
            return fallbackTitle;
        }

        int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
        if (nameIndex == -1) {
            return fallbackTitle;
        }

        if (!cursor.moveToFirst()) {
            return fallbackTitle;
        }

        String title = cursor.getString(nameIndex);
        if (TextUtils.isEmpty(title)) {
            return fallbackTitle;
        }

        return title;
    }

    private @Nullable String fetchMimeType() {
        return mContentResolver.getType(mUri);
    }

    private @Nullable Long fetchSize(@Nullable Cursor cursor) {
        if (cursor == null) {
            return null;
        }

        int sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE);
        if (sizeIndex == -1) {
            return null;
        }

        if (!cursor.moveToFirst()) {
            return null;
        }

        if (cursor.isNull(sizeIndex)) {
            return null;
        }

        return cursor.getLong(sizeIndex);
    }

    /**
     * Reads the attachment payload from {@link #mUri}, validating that the bytes look complete and
     * (for downscalable images) decodable before handing them to the upload pipeline.
     *
     * <p>The content URI is opened at most twice (once, plus one retry on an empty/short read). The
     * same in-memory bytes are reused for the C2PA sniff and for downscaling, so pipe-backed
     * providers (e.g. cloud media that "needs to be prepared") are not re-streamed.
     *
     * @param mimeType Resolved mime type of the attachment.
     * @param expectedSize Size reported by the content provider, if known.
     * @return The payload to upload, or null if the data could not be read or is unusable.
     */
    private byte @Nullable [] fetchData(String mimeType, @Nullable Long expectedSize) {
        @MimeTypeUtils.Type int fileType = MimeTypeUtils.getTypeFromMimeType(mimeType);
        ReadOutcome read = readAttachmentBytes(expectedSize);
        boolean oomOccurred = read.mResult == FuseboxAttachmentReadResult.OOM;
        @FuseboxAttachmentReadResult int result = read.mResult;
        byte[] data = read.mData;

        @Nullable CompressFormat outputFormat = getCompressionFormat(mimeType);
        boolean downscalingEnabled =
                outputFormat != null && OmniboxFeatures.sOmniboxAimImageDownscaling.isEnabled();

        if (data != null && downscalingEnabled) {
            boolean hasC2pa = C2paMetadataUtils.hasC2paMetadata(data);
            FuseboxMetrics.recordAttachmentC2paDetected(hasC2pa);

            if (!hasC2pa) {
                try {
                    data = loadDownscaledImage(data, assumeNonNull(outputFormat));
                } catch (OutOfMemoryError e) {
                    // Keep the raw bytes; native may still manage to process them.
                    Log.w(TAG, "OOM while downscaling attachment; using original bytes", e);
                    oomOccurred = true;
                } catch (IOException | IllegalArgumentException e) {
                    // We hold the complete payload and the platform decoder rejected it. Native
                    // decoding will fail the same way, so fail fast here with a precise reason
                    // rather than surfacing a generic IMAGE_PROCESSING_ERROR later.
                    Log.w(TAG, "Attachment bytes are not decodable (%d bytes)", data.length, e);
                    result = FuseboxAttachmentReadResult.DECODE_FAILED;
                    data = null;
                }
            }
        }

        FuseboxMetrics.recordAttachmentLoadOom(oomOccurred, fileType);
        FuseboxMetrics.recordAttachmentReadResult(result, fileType);
        return data;
    }

    /**
     * Reads the full byte stream behind {@link #mUri}, retrying once if the provider returns
     * nothing or fewer bytes than it advertised.
     */
    private ReadOutcome readAttachmentBytes(@Nullable Long expectedSize) {
        byte @Nullable [] best = null;

        for (int attempt = 0; attempt < MAX_READ_ATTEMPTS; attempt++) {
            byte @Nullable [] data;
            try (InputStream inputStream = mContentResolver.openInputStream(mUri)) {
                data = inputStream == null ? null : sFileStreamReader.readStream(inputStream);
            } catch (IOException e) {
                Log.w(TAG, "Failed to read attachment data", e);
                // A failed retry must not discard bytes already captured on an earlier attempt
                // (e.g. one-shot pipe-backed providers that cannot be reopened).
                if (best != null && best.length > 0) break;
                return new ReadOutcome(null, FuseboxAttachmentReadResult.IO_ERROR);
            } catch (OutOfMemoryError e) {
                Log.w(TAG, "Failed to read attachment data", e);
                if (best != null && best.length > 0) break;
                return new ReadOutcome(null, FuseboxAttachmentReadResult.OOM);
            }

            if (data != null && (best == null || data.length > best.length)) {
                best = data;
            }

            if (best != null && best.length > 0 && !isShortRead(best.length, expectedSize)) {
                return new ReadOutcome(
                        best,
                        attempt > 0
                                ? FuseboxAttachmentReadResult.SUCCESS_AFTER_RETRY
                                : FuseboxAttachmentReadResult.SUCCESS);
            }

            Log.w(
                    TAG,
                    "Attachment read incomplete (attempt %d): got %d bytes, expected %s",
                    attempt + 1,
                    data == null ? 0 : data.length,
                    expectedSize == null ? "unknown" : expectedSize.toString());
        }

        if (best == null || best.length == 0) {
            return new ReadOutcome(null, FuseboxAttachmentReadResult.EMPTY);
        }

        // Short, but non-empty. The provider-reported size is not always authoritative (e.g.
        // cloud-backed media), so forward what we have and let the decoder decide.
        return new ReadOutcome(best, FuseboxAttachmentReadResult.SHORT_READ);
    }

    private static boolean isShortRead(int actualLength, @Nullable Long expectedSize) {
        return expectedSize != null && expectedSize > 0 && actualLength < expectedSize;
    }

    /**
     * Decodes {@code source} with a bounded target size and re-encodes it as {@code outputFormat}.
     *
     * @throws IOException if the bytes cannot be decoded.
     * @throws IllegalArgumentException if the decoder rejects the source.
     * @throws OutOfMemoryError if allocation fails.
     */
    private byte[] loadDownscaledImage(byte[] source, CompressFormat outputFormat)
            throws IOException {
        Bitmap bitmap =
                sImageDecoder.decodeBitmap(
                        ImageDecoder.createSource(ByteBuffer.wrap(source)),
                        FuseboxAttachmentDetailsFetcher::setDecoderForDownscaling);

        try {
            ByteArrayOutputStream stream = new ByteArrayOutputStream();
            bitmap.compress(outputFormat, /* quality= */ 100, stream);
            return stream.toByteArray();
        } finally {
            bitmap.recycle();
        }
    }

    private static void setDecoderForDownscaling(
            ImageDecoder decoder, ImageDecoder.ImageInfo info, ImageDecoder.Source source) {
        decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE);

        Size size = info.getSize();
        Size targetSize = getTargetSizeForDownscaling(size.getWidth(), size.getHeight());
        if (targetSize == null) return;

        decoder.setTargetSize(targetSize.getWidth(), targetSize.getHeight());
    }

    /**
     * Calculate the target size to downscale an image to, preserving aspect ratio, so that its
     * total area does not exceed {@link #MAX_IMAGE_AREA}.
     *
     * @param width The original width of the image.
     * @param height The original height of the image.
     * @return The target {@link Size} if downscaling is required (the original area exceeds the
     *     limit and target dimensions are valid), or {@code null} if no downscaling is needed or if
     *     the downscaled dimensions would round down to 0.
     */
    @VisibleForTesting
    static @Nullable Size getTargetSizeForDownscaling(int width, int height) {
        long area = (long) width * height;
        if (area <= MAX_IMAGE_AREA) {
            return null;
        }

        double ratio = Math.sqrt((double) MAX_IMAGE_AREA / area);
        int targetWidth = (int) (width * ratio);
        int targetHeight = (int) (height * ratio);

        if (targetWidth <= 0 || targetHeight <= 0) {
            return null;
        }

        return new Size(targetWidth, targetHeight);
    }

    private static @Nullable CompressFormat getCompressionFormat(String mimeType) {
        return switch (mimeType) {
            case MimeTypeUtils.IMAGE_JPEG_MIME_TYPE, MimeTypeUtils.IMAGE_JPG_MIME_TYPE ->
                    CompressFormat.JPEG;
            case MimeTypeUtils.IMAGE_PNG_MIME_TYPE -> CompressFormat.PNG;
            default -> null;
        };
    }

    private @Nullable Drawable fetchThumbnail(byte[] data, String mimeType) {
        Drawable thumbnail = null;
        try {
            thumbnail =
                    new BitmapDrawable(
                            mContext.getResources(),
                            mContentResolver.loadThumbnail(
                                    mUri,
                                    new Size(
                                            THUMBNAIL_BITMAP_EDGE_SIZE, THUMBNAIL_BITMAP_EDGE_SIZE),
                                    null));
        } catch (IOException e) {
            // Ignore.
        }

        // If the thumbnail is still null, try to generate it directly from the loaded image data.
        if (MimeTypeUtils.getTypeFromMimeType(mimeType) == MimeTypeUtils.Type.IMAGE
                && thumbnail == null
                && data != null
                && data.length > 0) {
            ImageDimensions dims = getBitmapDimensionsFromBytes(data);

            // Downsample the image to save memory. The downsampled image size should be no
            // smaller than the standard thumbnail size to avoid upsampling later.
            int ratio = Math.min(dims.mHeight, dims.mWidth) / THUMBNAIL_BITMAP_EDGE_SIZE;
            int inSampleSize = Math.max(1, Integer.highestOneBit(ratio));

            @Nullable Bitmap bitmap = getBitmapFromBytes(data, inSampleSize);
            thumbnail = bitmap != null ? new BitmapDrawable(mContext.getResources(), bitmap) : null;
        }

        return thumbnail;
    }

    private static long getMaxSizeLimit(boolean isMetered) {
        return isMetered ? MAX_ATTACHMENT_SIZE_BYTES_ON_METERED_NETWORK : MAX_ATTACHMENT_SIZE_BYTES;
    }

    private static void recordAttachmentSizeLimitCheck(boolean isMetered, boolean isTooLarge) {
        FuseboxMetrics.notifyAttachmentSizeLimitCheck(
                isTooLarge
                        ? (isMetered
                                ? FuseboxAttachmentSizeLimitCheck.OVER_LIMIT_ON_METERED
                                : FuseboxAttachmentSizeLimitCheck.OVER_LIMIT_ON_UNMETERED)
                        : (isMetered
                                ? FuseboxAttachmentSizeLimitCheck.UNDER_LIMIT_ON_METERED
                                : FuseboxAttachmentSizeLimitCheck.UNDER_LIMIT_ON_UNMETERED));
    }

    private static void recordAttachmentSize(long size, String mimeType) {
        FuseboxMetrics.notifyFileAttachmentSize(size, MimeTypeUtils.getTypeFromMimeType(mimeType));
    }

    private static ImageDimensions getBitmapDimensionsFromBytes(byte[] data) {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inJustDecodeBounds = true;
        sBitmapDecoder.decodeByteArray(data, /* offset= */ 0, /* length= */ data.length, options);
        ImageDimensions dims = new ImageDimensions();
        dims.mWidth = options.outWidth;
        dims.mHeight = options.outHeight;
        return dims;
    }

    private static @Nullable Bitmap getBitmapFromBytes(byte[] data, int inSampleSize) {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inSampleSize = inSampleSize;
        try {
            return sBitmapDecoder.decodeByteArray(
                    data, /* offset= */ 0, /* length= */ data.length, options);
        } catch (OutOfMemoryError e) {
            Log.w(TAG, "Failed to generate attachment thumbnail", e);
            return null;
        }
    }

    @VisibleForTesting
    interface BitmapDecoder {
        @Nullable Bitmap decodeByteArray(
                byte[] data, int offset, int length, BitmapFactory.Options options)
                throws OutOfMemoryError;
    }

    @VisibleForTesting
    interface FileStreamReader {
        byte[] readStream(InputStream inputStream) throws IOException, OutOfMemoryError;
    }

    @VisibleForTesting
    interface DownscaledImageDecoder {
        Bitmap decodeBitmap(
                ImageDecoder.Source source, ImageDecoder.OnHeaderDecodedListener listener)
                throws IOException;
    }

    private static final class ImageDimensions {
        int mWidth;
        int mHeight;
    }

    /** Result of reading the attachment bytes, paired with the reason it ended that way. */
    private static final class ReadOutcome {
        final byte @Nullable [] mData;
        final @FuseboxAttachmentReadResult int mResult;

        ReadOutcome(byte @Nullable [] data, @FuseboxAttachmentReadResult int result) {
            mData = data;
            mResult = result;
        }
    }

    static void setBitmapDecoderForTesting(BitmapDecoder decoder) {
        sBitmapDecoder = decoder;
        ResettersForTesting.register(() -> sBitmapDecoder = BitmapFactory::decodeByteArray);
    }

    static void setFileStreamReaderForTesting(FileStreamReader reader) {
        sFileStreamReader = reader;
        ResettersForTesting.register(() -> sFileStreamReader = FileUtils::readStream);
    }

    static void setImageDecoderForTesting(DownscaledImageDecoder decoder) {
        sImageDecoder = decoder;
        ResettersForTesting.register(() -> sImageDecoder = ImageDecoder::decodeBitmap);
    }
}
