// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;


import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.StringRes;

import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link AccountSelectionProperties} changes in a {@link PropertyModel}
 * to the suitable method in {@link AccountSelectionView}.
 */
class AccountSelectionViewBinder {
    /**
     * Called whenever an account is bound to this view.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindAccountView(PropertyModel model, View view, PropertyKey key) {
        Account account = model.get(AccountProperties.ACCOUNT);
        if (key == AccountProperties.FAVICON_OR_FALLBACK) {
            ImageView imageView = view.findViewById(R.id.favicon);
            AccountProperties.FaviconOrFallback data =
                    model.get(AccountProperties.FAVICON_OR_FALLBACK);
            imageView.setImageDrawable(FaviconUtils.getIconDrawableWithoutFilter(data.mIcon,
                    data.mUrl, data.mFallbackColor,
                    FaviconUtils.createCircularIconGenerator(view.getResources()),
                    view.getResources(), data.mIconSize));
        } else if (key == AccountProperties.ON_CLICK_LISTENER) {
            view.setOnClickListener(clickedView -> {
                model.get(AccountProperties.ON_CLICK_LISTENER).onResult(account);
            });
        } else if (key == AccountProperties.ACCOUNT) {
            TextView subject = view.findViewById(R.id.name);
            subject.setText(account.getName());
            TextView email = view.findViewById(R.id.email);
            email.setText(account.getEmail());
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    /**
     * Called whenever a continue button for a single account is bound to this view.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindContinueButtonView(PropertyModel model, View view, PropertyKey key) {
        if (key == ContinueButtonProperties.ON_CLICK_LISTENER
                || key == ContinueButtonProperties.ACCOUNT) {
            view.setOnClickListener(clickedView -> {
                model.get(ContinueButtonProperties.ON_CLICK_LISTENER)
                        .onResult(model.get(ContinueButtonProperties.ACCOUNT));
            });
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    /**
     * Called whenever a header is bound to this view.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindHeaderView(PropertyModel model, View view, PropertyKey key) {
        if (key == HeaderProperties.SINGLE_ACCOUNT || key == HeaderProperties.FORMATTED_URL) {
            TextView sheetTitleText = view.findViewById(R.id.account_selection_sheet_title);
            @StringRes
            int titleStringId;
            if (model.get(HeaderProperties.SINGLE_ACCOUNT)) {
                titleStringId = R.string.account_selection_sheet_title_single;
            } else {
                titleStringId = R.string.account_selection_sheet_title;
            }

            String title = String.format(view.getContext().getString(titleStringId),
                    model.get(HeaderProperties.FORMATTED_URL));
            sheetTitleText.setText(title);
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    private AccountSelectionViewBinder() {}
}
