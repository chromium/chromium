// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
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
import org.chromium.base.task.AsyncTask;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentSizeLimitCheck;
import org.chromium.ui.base.MimeTypeUtils;

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

    private static final int THUMBNAIL_BITMAP_EDGE_SIZE = 256;

    @VisibleForTesting
    static final long MAX_ATTACHMENT_SIZE_BYTES = 100 * 1000 * 1000L; /* 100 MB */

    @VisibleForTesting
    static final long MAX_ATTACHMENT_SIZE_BYTES_ON_METERED_NETWORK = 20 * 1000 * 1000L; /* 20 MB */

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

    private static final class ImageDimensions {
        int mWidth;
        int mHeight;
    }

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

        String title = null;
        Long size = null;
        Cursor cursor =
                mContentResolver.query(
                        mUri,
                        /* projection= */ null,
                        /* selection= */ null,
                        /* selectionArgs= */ null,
                        /* sortOrder= */ null);
        if (cursor != null) {
            int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
            int sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE);
            cursor.moveToFirst();
            if (!cursor.isAfterLast()) {
                title = cursor.getString(nameIndex);
                if (sizeIndex != -1 && !cursor.isNull(sizeIndex)) {
                    size = cursor.getLong(sizeIndex);
                }
            }
            cursor.close();
        }

        if (TextUtils.isEmpty(title)) {
            title = mUri.getLastPathSegment();
        }

        String mimeType = mContentResolver.getType(mUri);

        if (size != null && mimeType != null) {
            FuseboxMetrics.notifyFileAttachmentSize(
                    size, MimeTypeUtils.getTypeFromMimeType(mimeType));
        }

        // Bail: don't add the item if we miss metadata.
        if (TextUtils.isEmpty(title) || TextUtils.isEmpty(mimeType)) return false;

        boolean isMetered = DeviceConditions.isCurrentActiveNetworkMetered(mContext);
        long maxSize =
                isMetered
                        ? MAX_ATTACHMENT_SIZE_BYTES_ON_METERED_NETWORK
                        : MAX_ATTACHMENT_SIZE_BYTES;

        /* Only exempt images from size limits, as they should be downscaled */
        if (MimeTypeUtils.getTypeFromMimeType(mimeType) != MimeTypeUtils.Type.IMAGE
                && (size == null || size > maxSize)) {

            if (size == null) return false;

            FuseboxMetrics.notifyAttachmentSizeLimitCheck(
                    isMetered
                            ? FuseboxAttachmentSizeLimitCheck.OVER_LIMIT_ON_METERED
                            : FuseboxAttachmentSizeLimitCheck.OVER_LIMIT_ON_UNMETERED);

            return false;
        }

        FuseboxMetrics.notifyAttachmentSizeLimitCheck(
                isMetered
                        ? FuseboxAttachmentSizeLimitCheck.UNDER_LIMIT_ON_METERED
                        : FuseboxAttachmentSizeLimitCheck.UNDER_LIMIT_ON_UNMETERED);

        byte[] data;

        try (InputStream inputStream = mContentResolver.openInputStream(mUri)) {
            if (inputStream == null) return false;
            data = FileUtils.readStream(inputStream);
        } catch (IOException e) {
            return false;
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

        mThumbnail = thumbnail;
        mTitle = title;
        mMimeType = mimeType;
        mData = data;

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

    private static ImageDimensions getBitmapDimensionsFromBytes(byte[] data) {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inJustDecodeBounds = true;
        BitmapFactory.decodeByteArray(data, /* offset= */ 0, /* length= */ data.length, options);
        ImageDimensions dims = new ImageDimensions();
        dims.mWidth = options.outWidth;
        dims.mHeight = options.outHeight;
        return dims;
    }

    private static @Nullable Bitmap getBitmapFromBytes(byte[] data, int inSampleSize) {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inSampleSize = inSampleSize;
        return BitmapFactory.decodeByteArray(
                data, /* offset= */ 0, /* length= */ data.length, options);
    }
}
