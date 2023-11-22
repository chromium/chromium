// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.browser_ui.util.BitmapCache;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.content.browser.contacts.ContactsPickerProperties;
import org.chromium.content_public.browser.ContactsPicker;
import org.chromium.content_public.browser.ContactsPickerListener;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.OptimizedFrameLayout;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * A class for keeping track of common data associated with showing contact details in the contacts
 * picker, for example the RecyclerView.
 */
public class PickerCategoryView extends OptimizedFrameLayout
        implements View.OnClickListener,
                RecyclerView.RecyclerListener,
                SelectionDelegate.SelectionObserver<ContactDetails>,
                SelectableListToolbar.SearchDelegate,
                TopView.SelectAllToggleCallback,
                CompressContactIconsWorkerTask.CompressContactIconsCallback {
    // These values are written to logs.  New enum values can be added, but existing
    // enums must never be renumbered or deleted and reused.
    private static final int ACTION_CANCEL = 0;
    private static final int ACTION_CONTACTS_SELECTED = 1;
    private static final int ACTION_BOUNDARY = 2;

    // Constants for the RoundedIconGenerator.
    private static final int ICON_SIZE_DP = 36;
    private static final int ICON_CORNER_RADIUS_DP = 20;
    private static final int ICON_TEXT_SIZE_DP = 12;

    // The dialog that owns us.
    private ContactsPickerDialog mDialog;

    // The view containing the RecyclerView and the toolbar, etc.
    private SelectableListLayout<ContactDetails> mSelectableListLayout;

    // The window for the main Activity.
    private WindowAndroid mWindowAndroid;

    // The callback to notify the listener of decisions reached in the picker.
    private ContactsPickerListener mListener;

    // The toolbar located at the top of the dialog.
    private ContactsPickerToolbar mToolbar;

    // The RecyclerView showing the images.
    private RecyclerView mRecyclerView;

    // The view at the top (showing the explanation and Select All checkbox).
    private TopView mTopView;

    // The {@link PickerAdapter} for the RecyclerView.
    private PickerAdapter mPickerAdapter;

    // The layout manager for the RecyclerView.
    private LinearLayoutManager mLayoutManager;

    // A helper class to draw the icon for each contact.
    private RoundedIconGenerator mIconGenerator;

    // The {@link SelectionDelegate} keeping track of which contacts are selected.
    private SelectionDelegate<ContactDetails> mSelectionDelegate;

    // A cache for contact images, lazily created.
    private ContactsBitmapCache mBitmapCache;

    // The search icon.
    private ImageView mSearchButton;

    // Keeps track of the set of last selected contacts in the UI.
    Set<ContactDetails> mPreviousSelection;

    // The Done text button that confirms the selection choice.
    private Button mDoneButton;

    // Whether the picker is in multi-selection mode.
    private boolean mMultiSelectionAllowed;

    // Whether the site is requesting names.
    private final boolean mSiteWantsNames;

    // Whether the site is requesting emails.
    private final boolean mSiteWantsEmails;

    // Whether the site is requesting telephone numbers.
    private final boolean mSiteWantsTel;

    // Whether the site is requesting addresses.
    private final boolean mSiteWantsAddresses;

    // Whether the site is requesting icons.
    private final boolean mSiteWantsIcons;

    /**
     * @param windowAndroid The Activity window the Contacts Picker is associated with.
     * @param adapter An uninitialized PickerAdapter for this dialog, which may contain
     *     embedder-specific behaviors. The PickerCategoryView will initialized it.
     * @param multiSelectionAllowed Whether the contacts picker should allow multiple items to be
     *     selected.
     * @param shouldIncludeNames Whether to allow sharing of names of contacts.
     * @param shouldIncludeEmails Whether to allow sharing of contact emails.
     * @param shouldIncludeTel Whether to allow sharing of contact telephone numbers.
     * @param shouldIncludeAddresses Whether to allow sharing of contact (physical) addresses.
     * @param shouldIncludeIcons Whether to allow sharing of contact icons.
     * @param formattedOrigin The origin receiving the contact details, formatted for display in the
     *     UI.
     * @param delegate A delegate listening for events from the toolbar.
     */
    @SuppressWarnings("unchecked") // mSelectableListLayout
    public PickerCategoryView(
            WindowAndroid windowAndroid,
            PickerAdapter adapter,
            boolean multiSelectionAllowed,
            boolean shouldIncludeNames,
            boolean shouldIncludeEmails,
            boolean shouldIncludeTel,
            boolean shouldIncludeAddresses,
            boolean shouldIncludeIcons,
            String formattedOrigin,
            ContactsPickerToolbar.ContactsToolbarDelegate delegate) {
        super(windowAndroid.getContext().get(), null);

        mWindowAndroid = windowAndroid;
        Context context = windowAndroid.getContext().get();
        mMultiSelectionAllowed = multiSelectionAllowed;
        mSiteWantsNames = shouldIncludeNames;
        mSiteWantsEmails = shouldIncludeEmails;
        mSiteWantsTel = shouldIncludeTel;
        mSiteWantsAddresses = shouldIncludeAddresses;
        mSiteWantsIcons = shouldIncludeIcons;

        mSelectionDelegate = new SelectionDelegate<ContactDetails>();
        if (!multiSelectionAllowed) mSelectionDelegate.setSingleSelectionMode();
        mSelectionDelegate.addObserver(this);

        Resources resources = context.getResources();
        int iconColor = context.getColor(R.color.default_favicon_background_color);
        mIconGenerator =
                new RoundedIconGenerator(
                        resources,
                        ICON_SIZE_DP,
                        ICON_SIZE_DP,
                        ICON_CORNER_RADIUS_DP,
                        iconColor,
                        ICON_TEXT_SIZE_DP);

        View root = LayoutInflater.from(context).inflate(R.layout.contacts_picker_dialog, this);
        mSelectableListLayout =
                (SelectableListLayout<ContactDetails>) root.findViewById(R.id.selectable_list);
        mSelectableListLayout.initializeEmptyView(R.string.contacts_picker_no_contacts_found);

        mPickerAdapter = adapter;
        mPickerAdapter.init(this, context, formattedOrigin);
        mRecyclerView = mSelectableListLayout.initializeRecyclerView(mPickerAdapter);
        int titleId =
                multiSelectionAllowed
                        ? R.string.contacts_picker_select_contacts
                        : R.string.contacts_picker_select_contact;
        mToolbar =
                (ContactsPickerToolbar)
                        mSelectableListLayout.initializeToolbar(
                                R.layout.contacts_picker_toolbar,
                                mSelectionDelegate,
                                titleId,
                                0,
                                0,
                                null,
                                false);
        mToolbar.setNavigationOnClickListener(this);
        mToolbar.initializeSearchView(this, R.string.contacts_picker_search, 0);
        mToolbar.setDelegate(delegate);
        mPickerAdapter.registerAdapterDataObserver(
                new RecyclerView.AdapterDataObserver() {
                    @Override
                    public void onChanged() {
                        updateSelectionState();
                    }
                });
        mSelectableListLayout.configureWideDisplayStyle();

        mSearchButton = (ImageView) mToolbar.findViewById(R.id.search);
        mSearchButton.setOnClickListener(this);
        mDoneButton = (Button) mToolbar.findViewById(R.id.done);
        mDoneButton.setOnClickListener(this);

        mLayoutManager = new LinearLayoutManager(context);
        mRecyclerView.setHasFixedSize(true);
        mRecyclerView.setLayoutManager(mLayoutManager);

        mBitmapCache = new ContactsBitmapCache();
    }

    /**
     * Initializes the PickerCategoryView object.
     *
     * @param dialog The dialog showing us.
     * @param listener The listener who should be notified of actions.
     */
    public void initialize(ContactsPickerDialog dialog, ContactsPickerListener listener) {
        mDialog = dialog;
        mListener = listener;

        mDialog.setOnCancelListener(
                dialog1 ->
                        executeAction(
                                ContactsPickerListener.ContactsPickerAction.CANCEL,
                                null,
                                ACTION_CANCEL));

        mPickerAdapter.notifyDataSetChanged();
    }

    public boolean siteWantsNames() {
        return mSiteWantsNames;
    }

    public boolean siteWantsEmails() {
        return mSiteWantsEmails;
    }

    public boolean siteWantsTel() {
        return mSiteWantsTel;
    }

    public boolean siteWantsAddresses() {
        return mSiteWantsAddresses;
    }

    public boolean siteWantsIcons() {
        return mSiteWantsIcons;
    }

    private void onStartSearch() {
        mDoneButton.setVisibility(GONE);

        // Showing the search clears current selection. Save it, so we can restore it after the
        // search has completed.
        mPreviousSelection = new HashSet<ContactDetails>(mSelectionDelegate.getSelectedItems());
        mSearchButton.setVisibility(GONE);
        mPickerAdapter.setSearchMode(true);
        mToolbar.showSearchView(true);
    }

    // SelectableListToolbar.SearchDelegate:

    @Override
    public void onEndSearch() {
        mPickerAdapter.setSearchString("");
        mPickerAdapter.setSearchMode(false);
        mToolbar.setNavigationOnClickListener(this);
        mDoneButton.setVisibility(VISIBLE);
        mSearchButton.setVisibility(VISIBLE);

        // Hiding the search view clears the selection. Save it first and restore to the old
        // selection, with the new item added during search.
        // TODO(finnur): This needs to be revisited after UX is finalized.
        HashSet<ContactDetails> selection = new HashSet<>();
        for (ContactDetails item : mSelectionDelegate.getSelectedItems()) {
            selection.add(item);
        }
        mToolbar.hideSearchView();
        for (ContactDetails item : mPreviousSelection) {
            selection.add(item);
        }

        // Post a runnable to update the selection so that the update occurs after the search fully
        // finishes, ensuring the number roll shows the right number.
        getHandler().post(() -> mSelectionDelegate.setSelectedItems(selection));
    }

    @Override
    public void onSearchTextChanged(String query) {
        mPickerAdapter.setSearchString(query);
    }

    // SelectionDelegate.SelectionObserver:

    @Override
    public void onSelectionStateChange(List<ContactDetails> selectedItems) {
        // Once a selection is made, drop out of search mode. Note: This function is also called
        // when entering search mode (with selectedItems then being 0 in size).
        if (mToolbar.isSearching() && selectedItems.size() > 0) {
            mToolbar.hideSearchView();
        }

        boolean allSelected = selectedItems.size() == mPickerAdapter.getItemCount() - 1;
        if (mTopView != null) mTopView.updateSelectAllCheckbox(allSelected);
    }

    // RecyclerView.RecyclerListener:

    @Override
    public void onViewRecycled(RecyclerView.ViewHolder holder) {
        ContactViewHolder bitmapHolder = (ContactViewHolder) holder;
        bitmapHolder.cancelIconRetrieval();
    }

    // TopView.SelectAllToggleCallback:

    @Override
    public void onSelectAllToggled(boolean allSelected) {
        if (allSelected) {
            mPreviousSelection = mSelectionDelegate.getSelectedItems();
            mSelectionDelegate.setSelectedItems(
                    new HashSet<ContactDetails>(mPickerAdapter.getAllContacts()));
            mListener.onContactsPickerUserAction(
                    ContactsPickerListener.ContactsPickerAction.SELECT_ALL,
                    /* contacts= */ null,
                    /* percentageShared= */ 0,
                    /* propertiesSiteRequested= */ 0,
                    /* propertiesUserRejected= */ 0);
        } else {
            mSelectionDelegate.setSelectedItems(new HashSet<ContactDetails>());
            mPreviousSelection = null;
            mListener.onContactsPickerUserAction(
                    ContactsPickerListener.ContactsPickerAction.UNDO_SELECT_ALL,
                    /* contacts= */ null,
                    /* percentageShared= */ 0,
                    /* propertiesSiteRequested= */ 0,
                    /* propertiesUserRejected= */ 0);
        }
    }

    // OnClickListener:

    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.done) {
            prepareContactsSelected();
        } else if (id == R.id.search) {
            onStartSearch();
        } else {
            executeAction(ContactsPickerListener.ContactsPickerAction.CANCEL, null, ACTION_CANCEL);
        }
    }

    // Simple getters and setters:

    SelectionDelegate<ContactDetails> getSelectionDelegate() {
        return mSelectionDelegate;
    }

    RoundedIconGenerator getIconGenerator() {
        return mIconGenerator;
    }

    ContactsBitmapCache getIconCache() {
        return mBitmapCache;
    }

    ModalDialogManager getModalDialogManager() {
        return mWindowAndroid.getModalDialogManager();
    }

    void setTopView(TopView topView) {
        mTopView = topView;
    }

    boolean multiSelectionAllowed() {
        return mMultiSelectionAllowed;
    }

    private void updateSelectionState() {
        boolean filterChipsSelected = mTopView == null || mTopView.filterChipsChecked() > 0;
        mToolbar.setFilterChipsSelected(filterChipsSelected);
    }

    /** Formats the selected contacts before notifying the listeners. */
    private void prepareContactsSelected() {
        List<ContactDetails> selectedContacts = mSelectionDelegate.getSelectedItemsAsList();
        Collections.sort(selectedContacts);

        if (mSiteWantsIcons && PickerAdapter.includesIcons()) {
            // Fetch missing icons and compress them first.
            new CompressContactIconsWorkerTask(
                            mWindowAndroid.getContext().get().getContentResolver(),
                            mBitmapCache,
                            selectedContacts,
                            this)
                    .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            return;
        }

        notifyContactsSelected(selectedContacts);
    }

    @Override
    public void iconsCompressed(List<ContactDetails> selectedContacts) {
        notifyContactsSelected(selectedContacts);
    }

    /**
     * @param isIncluded Whether the property was requested by the API.
     * @param isEnabled Whether the property was allowed to be shared by the user.
     * @param selected The property values that are currently selected.
     * @return The list of property values to share.
     */
    private <T> List<T> getContactPropertyValues(
            boolean isIncluded, boolean isEnabled, List<T> selected) {
        if (!isIncluded) {
            // The property wasn't requested in the API so return null.
            return null;
        }

        if (!isEnabled) {
            // The user doesn't want to share this property, so return an empty array.
            return new ArrayList<T>();
        }

        // Share whatever was selected.
        return selected;
    }

    /** Notifies any listeners that one or more contacts have been selected. */
    private void notifyContactsSelected(List<ContactDetails> selectedContacts) {
        List<ContactsPickerListener.Contact> contacts = new ArrayList<>();

        for (ContactDetails contactDetails : selectedContacts) {
            contacts.add(
                    new ContactsPickerListener.Contact(
                            getContactPropertyValues(
                                    mSiteWantsNames,
                                    PickerAdapter.includesNames(),
                                    contactDetails.getDisplayNames()),
                            getContactPropertyValues(
                                    mSiteWantsEmails,
                                    PickerAdapter.includesEmails(),
                                    contactDetails.getEmails()),
                            getContactPropertyValues(
                                    mSiteWantsTel,
                                    PickerAdapter.includesTelephones(),
                                    contactDetails.getPhoneNumbers()),
                            getContactPropertyValues(
                                    mSiteWantsAddresses,
                                    PickerAdapter.includesAddresses(),
                                    contactDetails.getAddresses()),
                            getContactPropertyValues(
                                    mSiteWantsIcons,
                                    PickerAdapter.includesIcons(),
                                    contactDetails.getIcons())));
        }

        executeAction(
                ContactsPickerListener.ContactsPickerAction.CONTACTS_SELECTED,
                contacts,
                ACTION_CONTACTS_SELECTED);
    }

    /**
     * Report back what the user selected in the dialog, report UMA and clean up.
     *
     * @param action The action taken.
     * @param contacts The contacts that were selected (if any).
     * @param umaId The UMA value to record with the action.
     */
    private void executeAction(
            @ContactsPickerListener.ContactsPickerAction int action,
            List<ContactsPickerListener.Contact> contacts,
            int umaId) {
        int selectCount = contacts != null ? contacts.size() : 0;
        int contactCount = mPickerAdapter.getAllContacts().size();
        int percentageShared = contactCount > 0 ? (100 * selectCount) / contactCount : 0;

        int propertiesSiteRequested = ContactsPickerProperties.PROPERTIES_NONE;
        int propertiesUserRejected = ContactsPickerProperties.PROPERTIES_NONE;
        if (mSiteWantsNames) {
            propertiesSiteRequested |= ContactsPickerProperties.PROPERTIES_NAMES;
            propertiesUserRejected |=
                    (!PickerAdapter.includesNames()
                            ? ContactsPickerProperties.PROPERTIES_NAMES
                            : ContactsPickerProperties.PROPERTIES_NONE);
        }
        if (mSiteWantsEmails) {
            propertiesSiteRequested |= ContactsPickerProperties.PROPERTIES_EMAILS;
            propertiesUserRejected |=
                    (!PickerAdapter.includesEmails()
                            ? ContactsPickerProperties.PROPERTIES_EMAILS
                            : ContactsPickerProperties.PROPERTIES_NONE);
        }
        if (mSiteWantsTel) {
            propertiesSiteRequested |= ContactsPickerProperties.PROPERTIES_TELS;
            propertiesUserRejected |=
                    (!PickerAdapter.includesTelephones()
                            ? ContactsPickerProperties.PROPERTIES_TELS
                            : ContactsPickerProperties.PROPERTIES_NONE);
        }
        if (mSiteWantsAddresses) {
            propertiesSiteRequested |= ContactsPickerProperties.PROPERTIES_ADDRESSES;
            propertiesUserRejected |=
                    (!PickerAdapter.includesAddresses()
                            ? ContactsPickerProperties.PROPERTIES_ADDRESSES
                            : ContactsPickerProperties.PROPERTIES_NONE);
        }
        if (mSiteWantsIcons) {
            propertiesSiteRequested |= ContactsPickerProperties.PROPERTIES_ICONS;
            propertiesUserRejected |=
                    (!PickerAdapter.includesIcons()
                            ? ContactsPickerProperties.PROPERTIES_ICONS
                            : ContactsPickerProperties.PROPERTIES_NONE);
        }

        mListener.onContactsPickerUserAction(
                action,
                contacts,
                percentageShared,
                propertiesSiteRequested,
                propertiesUserRejected);
        mDialog.dismiss();
        ContactsPicker.onContactsPickerDismissed();
        recordFinalUmaStats(
                umaId,
                contactCount,
                selectCount,
                percentageShared,
                propertiesSiteRequested,
                propertiesUserRejected);
    }

    /**
     * Record UMA statistics (what action was taken in the dialog and other performance stats).
     *
     * @param action The action the user took in the dialog.
     * @param contactCount The number of contacts in the contact list.
     * @param selectCount The number of contacts selected.
     * @param percentageShared The percentage shared (of the whole contact list).
     * @param propertiesRequested The properties (names/emails/tels) requested by the website.
     * @param propertiesRejected The properties (names/emails/tels) rejected by the user.
     */
    private void recordFinalUmaStats(
            int action,
            int contactCount,
            int selectCount,
            int percentageShared,
            int propertiesRequested,
            int propertiesRejected) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.ContactsPicker.DialogAction", action, ACTION_BOUNDARY);
        RecordHistogram.recordCount1MHistogram("Android.ContactsPicker.ContactCount", contactCount);
        RecordHistogram.recordCount1MHistogram("Android.ContactsPicker.SelectCount", selectCount);
        RecordHistogram.recordPercentageHistogram(
                "Android.ContactsPicker.SelectPercentage", percentageShared);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.ContactsPicker.PropertiesRequested",
                propertiesRequested,
                ContactsPickerProperties.PROPERTIES_BOUNDARY);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.ContactsPicker.PropertiesUserRejected",
                propertiesRejected,
                ContactsPickerProperties.PROPERTIES_BOUNDARY);
    }

    public SelectionDelegate<ContactDetails> getSelectionDelegateForTesting() {
        return mSelectionDelegate;
    }

    public TopView getTopViewForTesting() {
        return mTopView;
    }

    // A wrapper around BitmapCache to keep track of contacts that don't have an icon.
    protected static class ContactsBitmapCache {
        public BitmapCache bitmapCache;
        public Set<String> noIconIds;

        public ContactsBitmapCache() {
            // Each image (on a Pixel 2 phone) is about 30-40K. Calculate a proportional amount of
            // the available memory, but cap it at 5MB.
            final long maxMemory =
                    ConversionUtils.bytesToKilobytes(Runtime.getRuntime().maxMemory());
            int iconCacheSizeKb = (int) (maxMemory / 8); // 1/8th of the available memory.
            bitmapCache =
                    new BitmapCache(
                            GlobalDiscardableReferencePool.getReferencePool(),
                            Math.min(iconCacheSizeKb, 5 * ConversionUtils.BYTES_PER_MEGABYTE));

            noIconIds = new HashSet<>();
        }

        public Bitmap getBitmap(String id) {
            return bitmapCache.getBitmap(id);
        }

        public void putBitmap(String id, Bitmap icon) {
            if (icon == null) {
                noIconIds.add(id);
            } else {
                bitmapCache.putBitmap(id, icon);
                noIconIds.remove(id);
            }
        }
    }
}
