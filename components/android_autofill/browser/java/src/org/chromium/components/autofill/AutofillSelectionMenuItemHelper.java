// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.view.Menu;
import android.view.MenuItem;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content.R;
import org.chromium.content_public.browser.SelectionMenuItem;

import java.util.ArrayList;
import java.util.List;

/**
 * The class to provide autofill selection context menu items. To match the Android native view
 * behavior, the autofill context menu only appears when there is no text selected.
 */
@NullMarked
public class AutofillSelectionMenuItemHelper {
    private final AutofillProvider mAutofillProvider;
    private final int mAutofillMenuItemTitle;

    public AutofillSelectionMenuItemHelper(AutofillProvider autofillProvider) {
        mAutofillProvider = autofillProvider;
        mAutofillMenuItemTitle = android.R.string.autofill;
    }

    public List<SelectionMenuItem> getAdditionalItems() {
        List<SelectionMenuItem> autofillItems = new ArrayList<>();
        if (mAutofillProvider.shouldOfferPasskeyEntry()) {
            autofillItems.add(
                    new SelectionMenuItem.Builder(
                                    org.chromium.components.autofill.R.string
                                            .autofill_long_press_passkey_option)
                            .setId(R.id.select_action_menu_passkey_entry)
                            .setGroupId(R.id.select_action_menu_delegate_items)
                            .setOrderAndCategory(
                                    Menu.FIRST,
                                    SelectionMenuItem.ItemGroupOffset.SECONDARY_ASSIST_ITEMS)
                            .setShowAsActionFlags(
                                    MenuItem.SHOW_AS_ACTION_ALWAYS
                                            | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                            .build());
        }
        if (mAutofillMenuItemTitle != 0 && mAutofillProvider.shouldQueryAutofillSuggestion()) {
            autofillItems.add(
                    new SelectionMenuItem.Builder(mAutofillMenuItemTitle)
                            .setId(android.R.id.autofill)
                            .setGroupId(R.id.select_action_menu_delegate_items)
                            .setOrderAndCategory(
                                    Menu.CATEGORY_SECONDARY, // Show at end of section.
                                    SelectionMenuItem.ItemGroupOffset.SECONDARY_ASSIST_ITEMS)
                            .setShowAsActionFlags(
                                    MenuItem.SHOW_AS_ACTION_NEVER
                                            | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                            .build());
        }
        return autofillItems;
    }

    public boolean handleMenuItemClick(SelectionMenuItem item) {
        if (item.id == R.id.select_action_menu_passkey_entry) {
            mAutofillProvider.triggerPasskeyRequest();
            return true;
        } else if (item.id == android.R.id.autofill) {
            mAutofillProvider.queryAutofillSuggestion();
            return true;
        }
        return false;
    }
}
