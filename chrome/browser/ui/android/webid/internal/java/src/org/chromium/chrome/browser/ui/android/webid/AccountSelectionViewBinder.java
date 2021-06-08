// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.SINGLE_ACCOUNT;

import android.view.View;
import android.widget.TextView;

import androidx.annotation.StringRes;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link AccountSelectionProperties} changes in a {@link PropertyModel}
 * to the suitable method in {@link AccountSelectionView}.
 */
class AccountSelectionViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param key The {@link PropertyKey} which changed.
     */
    static void bindHeaderView(PropertyModel model, View view, PropertyKey key) {
        if (key == SINGLE_ACCOUNT || key == FORMATTED_URL) {
            TextView sheetTitleText = view.findViewById(R.id.account_selection_sheet_title);
            @StringRes
            int titleStringId;
            if (model.get(SINGLE_ACCOUNT)) {
                titleStringId = R.string.account_selection_sheet_title_single;
            } else {
                titleStringId = R.string.account_selection_sheet_title;
            }

            String title = String.format(
                    view.getContext().getString(titleStringId), model.get(FORMATTED_URL));
            sheetTitleText.setText(title);
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    private AccountSelectionViewBinder() {}
}
