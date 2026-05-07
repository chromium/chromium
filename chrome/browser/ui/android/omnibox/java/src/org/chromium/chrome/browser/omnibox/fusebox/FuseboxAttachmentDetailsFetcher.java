// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
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
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
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

        // Bail: don't add the item if we miss metadata.
        assert !TextUtils.isEmpty(title);
        assert !TextUtils.isEmpty(mimeType);
        if (title == null || mimeType == null) return false;

        /* Only exempt images from size limits, as they should be downscaled */
        if (MimeTypeUtils.getTypeFromMimeType(mimeType) != MimeTypeUtils.Type.IMAGE
                && (size == null || size > MAX_ATTACHMENT_SIZE_BYTES)) return false;

        byte[] data;

        try (InputStream inputStream = mContentResolver.openInputStream(mUri)) {
            if (inputStream == null) return false;
            data = FileUtils.readStream(inputStream);
        } catch (IOException e) {
            return false;
        }

        mThumbnail = thumbnail;
        mTitle = title;
        mMimeType = mimeType;
        mData = data;

        return true;
    }

    @Override
    protected void onPostExecute(Boolean result) {
        if (result == null || !result) {
            mCallback.onResult(/* result= */ null);
            return;
        }

        String mimeType = assumeNonNull(mMimeType);

        FuseboxAttachment attachment =
                switch (MimeTypeUtils.getTypeFromMimeType(mimeType)) {
                    case MimeTypeUtils.Type.IMAGE ->
                            mThumbnail != null
                                    ? FuseboxAttachment.forImage(
                                            mThumbnail,
                                            assumeNonNull(mTitle),
                                            mimeType,
                                            assumeNonNull(mData),
                                            mStartTime,
                                            mButtonType)
                                    : FuseboxAttachment.forImageNoThumbnail(
                                            assumeNonNull(mTitle),
                                            mimeType,
                                            assumeNonNull(mData),
                                            mStartTime,
                                            mButtonType);
                    case MimeTypeUtils.Type.PDF ->
                            FuseboxAttachment.forPdf(
                                    mThumbnail,
                                    assumeNonNull(mTitle),
                                    mimeType,
                                    assumeNonNull(mData),
                                    mStartTime,
                                    mButtonType);
                    default ->
                            FuseboxAttachment.forFile(
                                    mThumbnail,
                                    assumeNonNull(mTitle),
                                    mimeType,
                                    assumeNonNull(mData),
                                    mStartTime,
                                    mButtonType);
                };

        mCallback.onResult(attachment);
    }
}
