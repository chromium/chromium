// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import android.content.ContentResolver;

import org.chromium.base.task.AsyncTask;
import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.ContactsFetcher;

/** A class to retrieve contact data from Android Framework. */
@NullMarked
public class AndroidContactsFetcherImpl implements ContactsFetcher {
    // The content resolver to use for looking up contacts.
    private final ContentResolver mContentResolver;

    /**
     * A ContactsFetcherImpl constructor.
     *
     * @param contentResolver The ContentResolver to use for the lookup.
     */
    public AndroidContactsFetcherImpl(ContentResolver contentResolver) {
        mContentResolver = contentResolver;
    }

    // ContactsFetcher:

    @Override
    public AsyncTask fetchContacts(
            boolean includeNames,
            boolean includeEmails,
            boolean includeTel,
            boolean includeAddresses,
            ContactsFetcher.ContactsRetrievedCallback callback) {
        ContactsFetcherWorkerTask workerTask =
                new ContactsFetcherWorkerTask(
                        mContentResolver,
                        callback,
                        includeNames,
                        includeEmails,
                        includeTel,
                        includeAddresses);
        workerTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        return workerTask;
    }

    @Override
    public AsyncTask fetchIcon(
            String id, int iconSize, ContactsFetcher.IconRetrievedCallback callback) {
        FetchIconWorkerTask workerTask = new FetchIconWorkerTask(id, mContentResolver, callback);
        workerTask.setDesiredIconSize(iconSize);
        workerTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        return workerTask;
    }
}
