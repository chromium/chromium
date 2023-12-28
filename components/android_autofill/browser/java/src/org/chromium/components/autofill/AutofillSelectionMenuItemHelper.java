// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.content.Context;
import android.os.Build;
import android.view.Menu;
import android.view.MenuItem;

import org.chromium.content_public.browser.SelectionMenuItem;

import java.util.ArrayList;
import java.util.List;

/**
 * The class to provide autofill selection context menu items. To match the Android native view
 * behavior, the autofill context menu only appears when there is no text selected.
 */
public class AutofillSelectionMenuItemHelper {
    private final AutofillProvider mAutofillProvider;
    private final int mAutofillMenuItemTitle;

    // using getIdentifier to work around not-exposed framework resource ID
    @SuppressWarnings("DiscouragedApi")
    public AutofillSelectionMenuItemHelper(Context context, AutofillProvider autofillProvider) {
        mAutofillProvider = autofillProvider;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
            mAutofillMenuItemTitle = android.R.string.autofill;
        } else {
            // The string resource was not made public until O MR1, so on O we look it up by name.
            mAutofillMenuItemTitle =
                    context.getResources().getIdentifier("autofill", "string", "android");
        }
    }

    public List<SelectionMenuItem> getAdditionalItems() {
        List<SelectionMenuItem> autofillItems = new ArrayList<>();
        if (mAutofillMenuItemTitle == 0 || !mAutofillProvider.shouldQueryAutofillSuggestion()) {
            return autofillItems;
        }
        autofillItems.add(
                new SelectionMenuItem.Builder(mAutofillMenuItemTitle)
                        .setId(android.R.id.autofill)
                        .setOrderInCategory(Menu.CATEGORY_SECONDARY)
                        .setShowAsActionFlags(
                                MenuItem.SHOW_AS_ACTION_NEVER | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                        .setClickListener(v -> mAutofillProvider.queryAutofillSuggestion())
                        .build());
        return autofillItems;
    }
}
