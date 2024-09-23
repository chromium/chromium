// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.ON_DISMISSED;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.ON_QUERY_TEXT_CHANGE;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.PLUS_PROFILES;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.PlusProfileProperties.ON_PLUS_ADDRESS_SELECTED;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.PlusProfileProperties.PLUS_PROFILE;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.QUERY_HINT;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.TITLE;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.WARNING;

import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.autofill.helpers.FaviconHelper;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class AllPlusAddressesBottomSheetViewBinder {

    static void bindAllPlusAddressesBottomSheet(
            PropertyModel model, AllPlusAddressesBottomSheetView view, PropertyKey propertyKey) {
        if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else if (propertyKey == TITLE) {
            view.setTitle(model.get(TITLE));
        } else if (propertyKey == WARNING) {
            view.setWarning(model.get(WARNING));
        } else if (propertyKey == QUERY_HINT) {
            view.setQueryHint(model.get(QUERY_HINT));
        } else if (propertyKey == ON_QUERY_TEXT_CHANGE) {
            view.setOnQueryChangedCallback(model.get(ON_QUERY_TEXT_CHANGE));
        } else if (propertyKey == PLUS_PROFILES) {
            // Intentionally empty. The adapter will observe changes to PLUS_PROFILES.
        } else if (propertyKey == ON_DISMISSED) {
            view.setOnDismissedCallback(model.get(ON_DISMISSED));
        } else {
            assert false : "Every possible property update needs to be handled!";
        }
    }

    static View createPlusAddressView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.plus_profile_info_view, parent, false);
    }

    static void bindPlusAddressView(
            PropertyModel model, View view, PropertyKey propertyKey, FaviconHelper helper) {
        if (propertyKey == PLUS_PROFILE) {
            PlusProfile plusProfile = model.get(PLUS_PROFILE);

            TextView origin = view.findViewById(R.id.plus_profile_origin);
            origin.setText(plusProfile.getDisplayName());

            ChipView plusAddressChip = view.findViewById(R.id.plus_address);
            plusAddressChip.getPrimaryTextView().setText(plusProfile.getPlusAddress());
            plusAddressChip
                    .getPrimaryTextView()
                    .setContentDescription(plusProfile.getPlusAddress());

            setPlusProfileIcon(view, helper.getDefaultIcon(plusProfile.getOrigin()));
            helper.fetchFavicon(
                    plusProfile.getOrigin(),
                    (drawable) -> {
                        setPlusProfileIcon(view, drawable);
                    });
        } else if (propertyKey == ON_PLUS_ADDRESS_SELECTED) {
            ChipView plusAddressChip = view.findViewById(R.id.plus_address);
            plusAddressChip.setOnClickListener(
                    src ->
                            model.get(ON_PLUS_ADDRESS_SELECTED)
                                    .onResult(
                                            plusAddressChip
                                                    .getPrimaryTextView()
                                                    .getText()
                                                    .toString()));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private static void setPlusProfileIcon(View view, @Nullable Drawable icon) {
        if (icon == null) {
            return;
        }
        final int kIconSize =
                view.getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.all_plus_addresses_favicon_side);
        icon.setBounds(0, 0, kIconSize, kIconSize);
        ImageView plusProfileFavicon = view.findViewById(R.id.profile_origin_favicon);
        plusProfileFavicon.setImageDrawable(icon);
    }

    private AllPlusAddressesBottomSheetViewBinder() {}
}
