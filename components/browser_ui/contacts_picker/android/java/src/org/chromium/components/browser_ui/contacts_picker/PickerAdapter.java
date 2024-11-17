// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import android.content.ContentResolver;
import android.content.Context;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.CallSuper;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.Adapter;

import org.chromium.base.task.AsyncTask;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * A data adapter for the Contacts Picker.
 *
 * <p>This class is abstract and embedders must specialize it to provide access to the active user's
 * contact information.
 */
public abstract class PickerAdapter extends Adapter<RecyclerView.ViewHolder>
        implements ContactsFetcherWorkerTask.ContactsRetrievedCallback,
                TopView.ChipToggledCallback {
    /**
     * A ViewHolder for the top-most view in the RecyclerView. The view it contains has a checkbox
     * and some multi-line text that goes with it, so clicks on either text line should be treated
     * as clicks for the checkbox (hence the onclick forwarding).
     */
    static class TopViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener {
        TopView mItemView;

        public TopViewHolder(TopView itemView) {
            super(itemView);
            mItemView = itemView;
            mItemView.setOnClickListener(this);
        }

        @Override
        public void onClick(View view) {
            // TODO(finnur): Make the explanation text non-clickable.
            mItemView.toggle();
        }
    }

    /** The types of filters supported. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        FilterType.NAMES,
        FilterType.EMAILS,
        FilterType.TELEPHONES,
        FilterType.ADDRESSES,
        FilterType.ICONS
    })
    public @interface FilterType {
        int NAMES = 0;
        int EMAILS = 1;
        int TELEPHONES = 2;
        int ADDRESSES = 3;
        int ICONS = 4;
    }

    /** The types of views supported. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ViewType.SELECT_ALL_CHECKBOX, ViewType.CONTACT_DETAILS})
    private @interface ViewType {
        int SELECT_ALL_CHECKBOX = 0;
        int CONTACT_DETAILS = 1;
    }

    // The current context to use.
    private Context mContext;

    // The category view to use to show the contacts.
    private PickerCategoryView mCategoryView;

    // The view at the top of the RecyclerView (disclaimer and select all functionality).
    private TopView mTopView;

    // The origin the data will be shared with, formatted for display with the scheme omitted.
    private String mFormattedOrigin;

    // The content resolver to query data from.
    private ContentResolver mContentResolver;

    // The full list of all registered contacts on the device.
    private ArrayList<ContactDetails> mContactDetails;

    // The email address of the owner of the device.
    @Nullable private String mOwnerEmail;

    // The async worker task to use for fetching the contact details.
    private ContactsFetcherWorkerTask mWorkerTask;

    // Whether the user has switched to search mode.
    private boolean mSearchMode;

    // A list of search result indices into the larger data set.
    private ArrayList<Integer> mSearchResults;

    // Whether to include addresses in the returned results.
    private static boolean sIncludeAddresses;

    // Whether to include names in the returned results.
    private static boolean sIncludeNames;

    // Whether to include emails in the returned results.
    private static boolean sIncludeEmails;

    // Whether to include telephone numbers in the returned results.
    private static boolean sIncludeTelephones;

    // Whether to include icons in the returned results.
    private static boolean sIncludeIcons;

    // A list of contacts to use for testing (instead of querying Android).
    private static ArrayList<ContactDetails> sTestContacts;

    // An owner email to use when testing.
    private static String sTestOwnerEmail;

    /**
     * The PickerAdapter constructor.
     *
     * @param categoryView The category view to use to show the contacts.
     * @param context The current context.
     * @param formattedOrigin The origin the data will be shared with.
     */
    @CallSuper
    public void init(PickerCategoryView categoryView, Context context, String formattedOrigin) {
        mContext = context;
        mCategoryView = categoryView;
        mContentResolver = context.getContentResolver();
        mFormattedOrigin = formattedOrigin;
        sIncludeAddresses = true;
        sIncludeNames = true;
        sIncludeEmails = true;
        sIncludeTelephones = true;
        sIncludeIcons = true;

        if (getAllContacts() == null && sTestContacts == null) {
            mWorkerTask =
                    new ContactsFetcherWorkerTask(
                            context,
                            this,
                            mCategoryView.siteWantsNames(),
                            mCategoryView.siteWantsEmails(),
                            mCategoryView.siteWantsTel(),
                            mCategoryView.siteWantsAddresses());
            mWorkerTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        } else {
            contactsRetrieved(sTestContacts);
        }
    }

    /**
     * Set whether the user has switched to search mode.
     *
     * @param searchMode True when we are in search mode.
     */
    public void setSearchMode(boolean searchMode) {
        mSearchMode = searchMode;
        notifyDataSetChanged();
    }

    /**
     * Sets the search query (filter) for the contact list. Filtering is by display name.
     *
     * @param query The search term to use.
     */
    public void setSearchString(String query) {
        if (query.equals("")) {
            if (mSearchResults == null) return;
            mSearchResults.clear();
            mSearchResults = null;
        } else {
            mSearchResults = new ArrayList<Integer>();
            Integer count = 0;
            String query_lower = query.toLowerCase(Locale.getDefault());
            for (ContactDetails contact : mContactDetails) {
                if (contact.getDisplayName().toLowerCase(Locale.getDefault()).contains(query_lower)
                        || contact.getContactDetailsAsString(
                                        includesAddresses(), includesEmails(), includesTelephones())
                                .toLowerCase(Locale.getDefault())
                                .contains(query_lower)) {
                    mSearchResults.add(count);
                }
                count++;
            }
        }
        notifyDataSetChanged();
    }

    /**
     * Fetches all known contacts.
     *
     * @return The contact list as an array.
     */
    public ArrayList<ContactDetails> getAllContacts() {
        return mContactDetails;
    }

    protected String getOwnerEmail() {
        return mOwnerEmail;
    }

    protected void update() {
        if (mTopView != null) mTopView.updateContactCount(mContactDetails.size());
        notifyDataSetChanged();
    }

    // Abstract methods:

    /**
     * Called to get the email for the current user. The default is null, but some embedder-specific
     * specializations may override this method to facilitate showing the owner's contact card at
     * the top of the picker.
     *
     * @return the email address of the current user/owner.
     */
    @Nullable
    protected abstract String findOwnerEmail();

    /**
     * Called to add an entry which represents the current user to the given list. As with {@link
     * #findOwnerEmail}, embedders may override this to make sure the current user's contact card is
     * shown, or may no-op.
     *
     * @param contacts the list which is missing an entry for the active user, and to which such an
     *     entry should be prepended.
     */
    protected abstract void addOwnerInfoToContacts(ArrayList<ContactDetails> contacts);

    // ContactsFetcherWorkerTask.ContactsRetrievedCallback:

    @Override
    public void contactsRetrieved(ArrayList<ContactDetails> contacts) {
        mOwnerEmail = sTestOwnerEmail != null ? sTestOwnerEmail : findOwnerEmail();

        if (!processOwnerInfo(contacts, mOwnerEmail)) addOwnerInfoToContacts(contacts);
        mContactDetails = contacts;
        update();
    }

    // RecyclerView.Adapter:

    @Override
    public int getItemViewType(int position) {
        if (position == 0 && !mSearchMode) return ViewType.SELECT_ALL_CHECKBOX;
        return ViewType.CONTACT_DETAILS;
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        switch (viewType) {
            case ViewType.SELECT_ALL_CHECKBOX:
                mTopView =
                        (TopView)
                                LayoutInflater.from(parent.getContext())
                                        .inflate(R.layout.top_view, parent, false);
                mTopView.setSiteString(mFormattedOrigin);
                mTopView.registerSelectAllCallback(mCategoryView);
                mTopView.registerChipToggledCallback(this);
                mTopView.updateCheckboxVisibility(mCategoryView.multiSelectionAllowed());
                mTopView.updateChipVisibility(
                        mCategoryView.siteWantsNames(),
                        mCategoryView.siteWantsAddresses(),
                        mCategoryView.siteWantsEmails(),
                        mCategoryView.siteWantsTel(),
                        mCategoryView.siteWantsIcons());
                mCategoryView.setTopView(mTopView);
                if (mContactDetails != null) {
                    mTopView.updateContactCount(mContactDetails.size());
                }
                return new TopViewHolder(mTopView);
            case ViewType.CONTACT_DETAILS:
                ContactView itemView =
                        (ContactView)
                                LayoutInflater.from(parent.getContext())
                                        .inflate(R.layout.contact_view, parent, false);
                itemView.setCategoryView(mCategoryView);
                return new ContactViewHolder(
                        itemView,
                        mCategoryView,
                        mContentResolver,
                        mContext.getResources()
                                .getDimensionPixelSize(R.dimen.contact_picker_icon_size));
        }
        return null;
    }

    @Override
    public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {
        switch (holder.getItemViewType()) {
            case ViewType.SELECT_ALL_CHECKBOX:
                // There's no need to bind the Select All view.
                return;
            case ViewType.CONTACT_DETAILS:
                ContactViewHolder contactHolder = (ContactViewHolder) holder;
                ContactDetails contact;
                if (!mSearchMode || mSearchResults == null) {
                    // Subtract one because the first view is the Select All checkbox when not in
                    // search mode.
                    contact = mContactDetails.get(position - (mSearchMode ? 0 : 1));
                } else {
                    Integer index = mSearchResults.get(position);
                    contact = mContactDetails.get(index);
                }

                contactHolder.setContactDetails(contact);
        }
    }

    @Override
    // This will return how many items the RecyclerView should show, which can be a subset of
    // contacts when in search mode. This function also includes the Select All checkbox (which is
    // not a contact, obviously). To get the total number of contacts use getAllContacts().size()
    // instead.
    public int getItemCount() {
        if (mSearchResults != null) return mSearchResults.size();
        if (mContactDetails == null || mContactDetails.size() == 0) return 0;
        // Add one entry to account for the Select All checkbox, when not searching.
        return mContactDetails.size() + (mSearchMode ? 0 : 1);
    }

    // TopView.ChipToggledCallback:

    @Override
    public void onChipToggled(@FilterType int chip) {
        switch (chip) {
            case FilterType.NAMES:
                sIncludeNames = !sIncludeNames;
                break;
            case FilterType.ADDRESSES:
                sIncludeAddresses = !sIncludeAddresses;
                break;
            case FilterType.EMAILS:
                sIncludeEmails = !sIncludeEmails;
                break;
            case FilterType.TELEPHONES:
                sIncludeTelephones = !sIncludeTelephones;
                break;
            case FilterType.ICONS:
                sIncludeIcons = !sIncludeIcons;
                break;
            default:
                assert false;
        }

        notifyDataSetChanged();
    }

    /** Returns true unless the adapter is filtering out addresses. */
    public static boolean includesAddresses() {
        return sIncludeAddresses;
    }

    /** Returns true unless the adapter is filtering out names. */
    public static boolean includesNames() {
        return sIncludeNames;
    }

    /** Returns true unless the adapter is filtering out emails. */
    public static boolean includesEmails() {
        return sIncludeEmails;
    }

    /** Returns true unless the adapter is filtering out telephone numbers. */
    public static boolean includesTelephones() {
        return sIncludeTelephones;
    }

    /** Returns true unless the adapter is filtering out icons. */
    public static boolean includesIcons() {
        return sIncludeIcons;
    }

    /**
     * Sets a list of contacts to use as data for the dialog, and the owner email. For testing use
     * only.
     */
    @VisibleForTesting
    public static void setTestContactsAndOwner(
            ArrayList<ContactDetails> contacts, String ownerEmail) {
        sTestContacts = contacts;
        sTestOwnerEmail = ownerEmail;
    }

    /**
     * Attempts to figure out if the owner of the device is listed in the available contact details.
     * If so move it to the top of the list. If not found, returns false.
     *
     * @return Returns true if processing is complete, false if waiting on asynchronous fetching of
     *     missing data for the owner info.
     */
    private static boolean processOwnerInfo(ArrayList<ContactDetails> contacts, String ownerEmail) {
        if (ownerEmail == null) {
            return true;
        }

        ArrayList<Integer> matches = new ArrayList<Integer>();
        for (int i = 0; i < contacts.size(); ++i) {
            List<String> emails = contacts.get(i).getEmails();
            for (int y = 0; y < emails.size(); ++y) {
                if (TextUtils.equals(emails.get(y), ownerEmail)) {
                    matches.add(i);
                    break;
                }
            }
        }

        if (matches.size() == 0) {
            // No match was found, return false so that a record can be synthesized.
            return false;
        }

        // Move the contacts that match owner email to the top of the list.
        for (int i = 0; i < matches.size(); ++i) {
            int match = matches.get(i);
            ContactDetails contact = contacts.get(match);
            contact.setIsSelf(true);
            contacts.remove(match);
            contacts.add(i, contact);
        }
        return true;
    }
}
