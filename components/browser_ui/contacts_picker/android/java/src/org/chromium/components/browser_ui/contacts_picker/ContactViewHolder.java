// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import android.content.ContentResolver;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.AsyncTask;

/** Holds on to a {@link ContactView} that displays information about a contact. */
public class ContactViewHolder extends ViewHolder
        implements FetchIconWorkerTask.IconRetrievedCallback {
    // Our parent category.
    private final PickerCategoryView mCategoryView;

    // The Content Resolver to use for the lookup.
    private final ContentResolver mContentResolver;

    // The contact view we are holding on to.
    private final ContactView mItemView;

    // The details for the contact.
    private ContactDetails mContact;

    // A worker task for asynchronously retrieving icons off the main thread.
    private FetchIconWorkerTask mWorkerTask;

    // The size the contact icon will be displayed at (one side of a square).
    private final int mIconSize;

    // The icon to use when testing.
    private static Bitmap sIconForTest;

    /**
     * The PickerBitmapViewHolder.
     *
     * @param itemView The {@link ContactView} for the contact.
     * @param categoryView The {@link PickerCategoryView} showing the contacts.
     * @param contentResolver The {@link ContentResolver} to use for the lookup.
     * @param iconSize The size the contact icon will be displayed at (one side of a square).
     */
    public ContactViewHolder(
            ContactView itemView,
            PickerCategoryView categoryView,
            ContentResolver contentResolver,
            int iconSize) {
        super(itemView);
        mCategoryView = categoryView;
        mContentResolver = contentResolver;
        mItemView = itemView;
        mIconSize = iconSize;
    }

    /**
     * Sets the contact details to show in the itemview. If the image is not found in the cache, an
     * asynchronous worker task is created to load it.
     *
     * @param contact The contact details to show.
     */
    public void setContactDetails(ContactDetails contact) {
        mContact = contact;

        if (sIconForTest != null) {
            mItemView.initialize(contact, sIconForTest);
            return;
        }

        Drawable drawable = contact.getSelfIcon();
        if (drawable != null) {
            assert drawable instanceof BitmapDrawable;
            Bitmap bitmap = ((BitmapDrawable) drawable).getBitmap();
            mItemView.initialize(contact, bitmap);
        } else {
            Bitmap icon = mCategoryView.getIconCache().getBitmap(mContact.getId());
            if (icon == null && !contact.getId().equals(ContactDetails.SELF_CONTACT_ID)) {
                mWorkerTask = new FetchIconWorkerTask(mContact.getId(), mContentResolver, this);
                mWorkerTask.setDesiredIconSize(mIconSize);
                mWorkerTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            }
            mItemView.initialize(contact, icon);
        }
    }

    /** Cancels the worker task to retrieve the icon. */
    public void cancelIconRetrieval() {
        mWorkerTask.cancel(true);
        mWorkerTask = null;
    }

    // FetchIconWorkerTask.IconRetrievedCallback:

    @Override
    public void iconRetrieved(Bitmap icon, String contactId) {
        if (mCategoryView.getIconCache().getBitmap(contactId) == null) {
            mCategoryView.getIconCache().putBitmap(contactId, icon);
        }

        if (icon != null && contactId.equals(mContact.getId())) {
            mItemView.setIconBitmap(icon);
        }
    }

    /** Sets the icon to use when testing. */
    public static void setIconForTesting(Bitmap icon) {
        sIconForTest = icon;
        ResettersForTesting.register(() -> sIconForTest = null);
    }
}
