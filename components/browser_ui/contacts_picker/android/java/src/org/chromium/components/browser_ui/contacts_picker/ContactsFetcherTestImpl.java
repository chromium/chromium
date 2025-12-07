// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import android.graphics.Bitmap;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.ContactsFetcher;
import org.chromium.content_public.browser.ContactsFetcher.RetrievedContact;

import java.util.ArrayList;
import java.util.concurrent.Phaser;

/**
 * A ContactsFetcher implementation for testing. This returns stored contacts instead of querying
 * Android.
 */
@NullMarked
class ContactsFetcherTestImpl implements ContactsFetcher {
    private ArrayList<RetrievedContact> mTestContacts = new ArrayList<>();
    private @Nullable Bitmap mTestIcon;
    private final Phaser mPhaser = new Phaser(1);

    public void setTestContacts(ArrayList<RetrievedContact> contacts) {
        mTestContacts = contacts;
    }

    public void setTestIcon(Bitmap icon) {
        mTestIcon = icon;
    }

    public void awaitFetchIcon() {
        mPhaser.arriveAndAwaitAdvance();
    }

    @Override
    public @Nullable AsyncTask fetchContacts(
            boolean includeNames,
            boolean includeEmails,
            boolean includeTel,
            boolean includeAddresses,
            ContactsFetcher.ContactsRetrievedCallback callback) {
        callback.contactsRetrieved(mTestContacts);
        return null;
    }

    @Override
    public @Nullable AsyncTask fetchIcon(
            String id, int iconSize, ContactsFetcher.IconRetrievedCallback callback) {
        mPhaser.register();
        ThreadUtils.postOnUiThreadDelayed(
                () -> {
                    callback.iconRetrieved(mTestIcon, id);
                    mPhaser.arriveAndDeregister();
                },
                100);
        return null;
    }
}
