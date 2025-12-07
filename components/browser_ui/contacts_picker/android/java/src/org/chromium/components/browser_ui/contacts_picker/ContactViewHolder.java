// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.task.AsyncTask;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.ContactsFetcher;

/** Holds on to a {@link ContactView} that displays information about a contact. */
@NullMarked
public class ContactViewHolder extends ViewHolder implements ContactsFetcher.IconRetrievedCallback {
    // Our parent category.
    private final PickerCategoryView mCategoryView;

    // The contact view we are holding on to.
    private final ContactView mItemView;

    // The details for the contact.
    private ContactDetails mContact;

    // A worker task for asynchronously retrieving icons off the main thread.
    private @Nullable AsyncTask mWorkerTask;

    // The size the contact icon will be displayed at (one side of a square).
    private final int mIconSize;

    // An instance of {@link ContactsFetcher} to query data.
    private final ContactsFetcher mContactsFetcher;

    /**
     * The PickerBitmapViewHolder.
     *
     * @param itemView The {@link ContactView} for the contact.
     * @param categoryView The {@link PickerCategoryView} showing the contacts.
     * @param iconSize The size the contact icon will be displayed at (one side of a square).
     * @param contactsFetcher An instance of {@link ContactsFetcher} to query data.
     */
    public ContactViewHolder(
            ContactView itemView,
            PickerCategoryView categoryView,
            int iconSize,
            ContactsFetcher contactsFetcher) {
        super(itemView);
        mCategoryView = categoryView;
        mItemView = itemView;
        mIconSize = iconSize;
        mContactsFetcher = contactsFetcher;
    }

    /**
     * Sets the contact details to show in the itemview. If the image is not found in the cache, an
     * asynchronous worker task is created to load it.
     *
     * @param contact The contact details to show.
     */
    @Initializer
    public void setContactDetails(ContactDetails contact) {
        mContact = contact;

        Drawable drawable = contact.getSelfIcon();
        if (drawable != null) {
            assert drawable instanceof BitmapDrawable;
            Bitmap bitmap = ((BitmapDrawable) drawable).getBitmap();
            mItemView.initialize(contact, bitmap);
        } else {
            Bitmap icon = mCategoryView.getIconCache().getBitmap(mContact.getId());
            if (icon == null && !contact.getId().equals(ContactDetails.SELF_CONTACT_ID)) {
                mWorkerTask = mContactsFetcher.fetchIcon(mContact.getId(), mIconSize, this);
            }
            mItemView.initialize(contact, icon);
        }
    }

    /** Cancels the worker task to retrieve the icon. */
    public void cancelIconRetrieval() {
        if (mWorkerTask != null) {
            mWorkerTask.cancel(true);
            mWorkerTask = null;
        }
    }

    // FetchIconWorkerTask.IconRetrievedCallback:

    @Override
    public void iconRetrieved(@Nullable Bitmap icon, String contactId) {
        if (mCategoryView.getIconCache().getBitmap(contactId) == null) {
            mCategoryView.getIconCache().putBitmap(contactId, icon);
        }

        if (icon != null && contactId.equals(mContact.getId())) {
            mItemView.setIconBitmap(icon);
        }
    }
}
