// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.database.Cursor;
import android.net.Uri;
import android.provider.ContactsContract;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.ContactsFetcher;

/** A worker task to retrieve images for contacts. */
@NullMarked
class FetchIconWorkerTask extends AsyncTask<byte @Nullable []> {
    // The ID of the contact to look up.
    private final String mContactId;

    // If positive, the returned icon will be scaled to this size, measured along one side of a
    // square, in pixels. Otherwise, the returned image will be returned as-is.
    private int mDesiredIconSize;

    // The content resolver to use for looking up
    private final ContentResolver mContentResolver;

    // The callback to use to communicate the results.
    private final ContactsFetcher.IconRetrievedCallback mCallback;

    /**
     * A FetchIconWorkerTask constructor.
     *
     * @param id The id of the contact to look up.
     * @param contentResolver The ContentResolver to use for the lookup.
     * @param callback The callback to use to communicate back the results.
     */
    public FetchIconWorkerTask(
            String id,
            ContentResolver contentResolver,
            ContactsFetcher.IconRetrievedCallback callback) {
        mContactId = id;
        // Avatar icon for own info should not be obtained through the contacts list.
        assert !id.equals(ContactDetails.SELF_CONTACT_ID);
        mContentResolver = contentResolver;
        mCallback = callback;
    }

    /**
     * If called, {@link FetchIconWorkerTask} will scale the icon to the given size before returning
     * it.
     *
     * @param iconSize the size (both width and height) to scale to.
     */
    public void setDesiredIconSize(int iconSize) {
        mDesiredIconSize = iconSize;
    }

    /**
     * Fetches the icon of a particular contact (in a background thread).
     *
     * @return The icon data representing a contact (returned as byte[]).
     */
    @Override
    protected byte @Nullable [] doInBackground() {
        assert !ThreadUtils.runningOnUiThread();

        if (isCancelled()) return null;

        try {
            Uri contactUri =
                    ContentUris.withAppendedId(
                            ContactsContract.Contacts.CONTENT_URI, Long.parseLong(mContactId));
            Uri photoUri =
                    Uri.withAppendedPath(
                            contactUri, ContactsContract.Contacts.Photo.CONTENT_DIRECTORY);
            try (Cursor cursor =
                    mContentResolver.query(
                            photoUri,
                            new String[] {ContactsContract.Contacts.Photo.PHOTO},
                            null,
                            null,
                            null)) {
                if (cursor != null && cursor.moveToFirst()) {
                    byte @Nullable [] data = cursor.getBlob(0);
                    // Cap the photo size at 1 MB to prevent excessive memory usage.
                    if (data != null
                            && data.length <= ContactsPickerImageDecoder.MAX_IMAGE_SIZE_BYTES) {
                        return data;
                    }
                }
            }
        } catch (Exception e) {
            // Guard against SecurityException or SQLiteBlobTooBigException.
            return null;
        }
        return null;
    }

    /**
     * Communicates the results back to the client. Called on the UI thread.
     *
     * @param data The icon byte data retrieved.
     */
    @Override
    protected void onPostExecute(byte @Nullable [] data) {
        assert ThreadUtils.runningOnUiThread();

        if (isCancelled()) {
            return;
        }

        if (data == null || data.length == 0) {
            mCallback.iconRetrieved(null, mContactId);
            return;
        }

        ContactsPickerImageDecoder.decodeImage(
                data,
                mDesiredIconSize,
                (bitmap) -> {
                    if (isCancelled()) {
                        return;
                    }
                    mCallback.iconRetrieved(bitmap, mContactId);
                });
    }
}
