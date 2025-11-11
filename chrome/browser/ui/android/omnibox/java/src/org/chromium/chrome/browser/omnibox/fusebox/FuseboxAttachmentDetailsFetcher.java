// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.text.TextUtils;
import android.util.Size;

import org.chromium.base.Callback;
import org.chromium.base.FileUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;

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
class FuseboxAttachmentDetailsFetcher extends AsyncTask<@Nullable FuseboxAttachment> {

    private static final int THUMBNAIL_BITMAP_EDGE_SIZE = 256;
    private final Context mContext;
    private final ContentResolver mContentResolver;
    private final Uri mUri;
    private final @FuseboxAttachmentType int mType;
    private final Callback<FuseboxAttachment> mCallback;

    FuseboxAttachmentDetailsFetcher(
            Context context,
            ContentResolver contentResolver,
            Uri uri,
            @FuseboxAttachmentType int type,
            Callback<FuseboxAttachment> callback) {
        mContext = context;
        mContentResolver = contentResolver;
        mUri = uri;
        mType = type;
        mCallback = callback;
    }

    @Override
    protected @Nullable FuseboxAttachment doInBackground() {
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
        Cursor cursor =
                mContentResolver.query(
                        mUri,
                        /* projection= */ null,
                        /* selection= */ null,
                        /* selectionArgs= */ null,
                        /* sortOrder= */ null);
        if (cursor != null) {
            int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
            cursor.moveToFirst();
            if (!cursor.isAfterLast()) {
                title = cursor.getString(nameIndex);
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
        if (title == null || mimeType == null) return null;

        byte[] data;

        try (InputStream inputStream = mContentResolver.openInputStream(mUri)) {
            if (inputStream == null) return null;
            data = FileUtils.readStream(inputStream);
        } catch (IOException e) {
            return null;
        }

        return new FuseboxAttachment(mType, thumbnail, title, mimeType, data);
    }

    @Override
    protected void onPostExecute(@Nullable FuseboxAttachment result) {
        if (result == null) return;
        mCallback.onResult(result);
    }
}
