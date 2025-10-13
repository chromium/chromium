// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.view.Menu;
import android.view.MenuItem;

import org.chromium.build.annotations.NullMarked;
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
                    new SelectionMenuItem.Builder(R.string.autofill_long_press_passkey_option)
                            .setId(Menu.NONE)
                            .setOrderInCategory(Menu.FIRST)
                            .setShowAsActionFlags(
                                    MenuItem.SHOW_AS_ACTION_ALWAYS
                                            | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                            .setClickListener(v -> mAutofillProvider.triggerPasskeyRequest())
                            .build());
        }
        if (mAutofillMenuItemTitle != 0 && mAutofillProvider.shouldQueryAutofillSuggestion()) {
            autofillItems.add(
                    new SelectionMenuItem.Builder(mAutofillMenuItemTitle)
                            .setId(android.R.id.autofill)
                            .setOrderInCategory(Menu.CATEGORY_SECONDARY)
                            .setShowAsActionFlags(
                                    MenuItem.SHOW_AS_ACTION_NEVER
                                            | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                            .setClickListener(v -> mAutofillProvider.queryAutofillSuggestion())
                            .build());
        }
        return autofillItems;
    }
}
