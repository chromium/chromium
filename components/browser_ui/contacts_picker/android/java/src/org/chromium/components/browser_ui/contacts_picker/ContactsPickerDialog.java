// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import org.chromium.components.browser_ui.widget.FullscreenAlertDialog;
import org.chromium.content_public.browser.ContactsPickerListener;
import org.chromium.ui.base.WindowAndroid;

/**
 * UI for the contacts picker that shows on the Android platform as a result of &lt;input type=file
 * accept=contacts &gt; form element.
 */
public class ContactsPickerDialog extends FullscreenAlertDialog
        implements ContactsPickerToolbar.ContactsToolbarDelegate {
    // The category we're showing contacts for.
    private PickerCategoryView mCategoryView;

    /**
     * The ContactsPickerDialog constructor.
     *
     * @param windowAndroid The window associated with the main Activity.
     * @param adapter An uninitialized {@link PickerAdapter} for this dialog.
     * @param listener The listener object that gets notified when an action is taken.
     * @param allowMultiple Whether the contacts picker should allow multiple items to be selected.
     * @param includeNames Whether the contacts data returned should include names.
     * @param includeEmails Whether the contacts data returned should include emails.
     * @param includeTel Whether the contacts data returned should include telephone numbers.
     * @param includeAddresses Whether the contacts data returned should include addresses.
     * @param includeIcons Whether the contacts data returned should include icons.
     * @param formattedOrigin The origin the data will be shared with, formatted for display with
     *     the scheme omitted.
     */
    public ContactsPickerDialog(
            WindowAndroid windowAndroid,
            PickerAdapter adapter,
            ContactsPickerListener listener,
            boolean allowMultiple,
            boolean includeNames,
            boolean includeEmails,
            boolean includeTel,
            boolean includeAddresses,
            boolean includeIcons,
            String formattedOrigin) {
        super(windowAndroid.getContext().get());

        // Initialize the main content view.
        mCategoryView =
                new PickerCategoryView(
                        windowAndroid,
                        adapter,
                        allowMultiple,
                        includeNames,
                        includeEmails,
                        includeTel,
                        includeAddresses,
                        includeIcons,
                        formattedOrigin,
                        this);
        mCategoryView.initialize(this, listener);
        setView(mCategoryView);
    }

    /** Cancels the dialog in response to a back navigation. */
    @Override
    public void onNavigationBackCallback() {
        cancel();
    }

    public PickerCategoryView getCategoryViewForTesting() {
        return mCategoryView;
    }
}
