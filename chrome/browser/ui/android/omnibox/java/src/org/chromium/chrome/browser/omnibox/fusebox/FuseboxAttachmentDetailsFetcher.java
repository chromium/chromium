// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

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
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentSizeLimitCheck;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.MimeTypeUtils;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

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
    private static final int MAX_IMAGE_EDGE_SIZE = 1600;

    @VisibleForTesting
    static final long MAX_ATTACHMENT_SIZE_BYTES = 100 * 1000 * 1000L; /* 100 MB */

    @VisibleForTesting
    static final long MAX_ATTACHMENT_SIZE_BYTES_ON_METERED_NETWORK = 20 * 1000 * 1000L; /* 20 MB */

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

        mData = fetchData(mMimeType);
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

    private byte @Nullable [] fetchData(String mimeType) {
        byte[] data = null;

        @Nullable CompressFormat outputFormat = getCompressionFormat(mimeType);

        if (outputFormat != null && OmniboxFeatures.sOmniboxAimImageDownscaling.isEnabled()) {
            try {
                data = loadDownscaledImage(outputFormat);
            } catch (OutOfMemoryError e) {
                Log.w(TAG, "Failed to read attachment data", e);
            }
        }

        if (data == null) {
            try (InputStream inputStream = mContentResolver.openInputStream(mUri)) {
                if (inputStream == null) return null;
                data = sFileStreamReader.readStream(inputStream);
            } catch (IOException | OutOfMemoryError e) {
                Log.w(TAG, "Failed to read attachment data", e);
                return null;
            }
        }

        return data;
    }

    private byte @Nullable [] loadDownscaledImage(CompressFormat outputFormat) {
        Bitmap bitmap;
        try {
            bitmap =
                    sImageDecoder.decodeBitmap(
                            ImageDecoder.createSource(mContentResolver, mUri),
                            FuseboxAttachmentDetailsFetcher::setDecoderForDownscaling);
        } catch (IOException | IllegalArgumentException e) {
            Log.w(TAG, "Failed to decode image from URI", e);
            return null;
        }

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
        int width = size.getWidth();
        int height = size.getHeight();

        if (width <= MAX_IMAGE_EDGE_SIZE && height <= MAX_IMAGE_EDGE_SIZE) {
            return;
        }

        double ratio = (double) MAX_IMAGE_EDGE_SIZE / Math.max(width, height);
        int targetWidth = (int) Math.round(width * ratio);
        int targetHeight = (int) Math.round(height * ratio);

        // This is highly likely for very asymmetrical images e.g., (20000 x 1). Just load the image
        // without downscaling.
        if (targetWidth <= 0 || targetHeight <= 0) {
            return;
        }

        decoder.setTargetSize(targetWidth, targetHeight);
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
}
