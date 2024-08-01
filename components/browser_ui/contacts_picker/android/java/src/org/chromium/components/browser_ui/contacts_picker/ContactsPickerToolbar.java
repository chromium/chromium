// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import android.content.Context;
import android.util.AttributeSet;

import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.widget.ButtonCompat;

import java.util.List;

/** Handles toolbar functionality for the {@ContactsPickerDialog}. */
public class ContactsPickerToolbar extends SelectableListToolbar<ContactDetails> {
    /** A delegate that handles dialog actions. */
    public interface ContactsToolbarDelegate {
        /** Called when the back arrow is clicked in the toolbar. */
        void onNavigationBackCallback();
    }

    // A delegate to notify when the dialog should close.
    private ContactsToolbarDelegate mDelegate;

    // Whether any filter chips are selected. Default to true because all filter chips are selected
    // by default when opening the dialog.
    private boolean mFilterChipsSelected = true;

    public ContactsPickerToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /** Set the {@ContactToolbarDelegate} for this toolbar. */
    public void setDelegate(ContactsToolbarDelegate delegate) {
        mDelegate = delegate;
    }

    /** Shows the Back arrow navigation button in the upper left corner. */
    public void showBackArrow() {
        setNavigationButton(NavigationButton.SEARCH_BACK);
    }

    /** Sets whether any filter chips are |selected| in the dialog. */
    public void setFilterChipsSelected(boolean selected) {
        mFilterChipsSelected = selected;
        updateToolbarUI();
    }

    // SelectableListToolbar:

    @Override
    public void onSearchNavigationBack() {
        if (isSearching()) {
            super.onSearchNavigationBack();
        } else {
            mDelegate.onNavigationBackCallback();
        }
    }

    @Override
    public void initialize(
            SelectionDelegate<ContactDetails> delegate,
            int titleResId,
            int normalGroupResId,
            int selectedGroupResId,
            boolean updateStatusBarColor) {
        super.initialize(
                delegate, titleResId, normalGroupResId, selectedGroupResId, updateStatusBarColor);

        showBackArrow();
    }

    @Override
    public void onSelectionStateChange(List<ContactDetails> selectedItems) {
        super.onSelectionStateChange(selectedItems);
        updateToolbarUI();
    }

    /** Update the UI elements of the toolbar, based on whether contacts & filter chips are selected. */
    private void updateToolbarUI() {
        boolean contactsSelected = !mSelectionDelegate.getSelectedItems().isEmpty();

        boolean doneEnabled = contactsSelected && mFilterChipsSelected;
        ButtonCompat done = findViewById(R.id.done);
        done.setEnabled(doneEnabled);

        if (doneEnabled) {
            done.setTextAppearance(R.style.TextAppearance_TextMedium_Secondary);
        } else {
            done.setTextAppearance(R.style.TextAppearance_TextMedium_Disabled);
            if (contactsSelected) {
                setNavigationButton(NavigationButton.SELECTION_BACK);
            } else {
                showBackArrow();
            }
        }
    }
}
