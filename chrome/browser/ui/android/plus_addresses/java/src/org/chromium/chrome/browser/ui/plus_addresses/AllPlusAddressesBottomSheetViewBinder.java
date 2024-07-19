// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.PLUS_PROFILES;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.PlusProfileProperties.PLUS_PROFILE;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.TITLE;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.VISIBLE;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

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
        } else if (propertyKey == PLUS_PROFILES) {
            // Intentionally empty. The adapter will observe changes to PLUS_PROFILES.
        } else {
            assert false : "Every possible property update needs to be handled!";
        }
    }

    static View createPlusAddressView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.plus_profile_info_view, parent, false);
    }

    static void bindPlusAddressView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == PLUS_PROFILE) {
            PlusProfile plusProfile = model.get(PLUS_PROFILE);

            TextView origin = view.findViewById(R.id.plus_profile_origin);
            origin.setText(plusProfile.getOrigin());

            ChipView plusAddressChip = view.findViewById(R.id.plus_address);
            plusAddressChip.getPrimaryTextView().setText(plusProfile.getPlusAddress());
            plusAddressChip
                    .getPrimaryTextView()
                    .setContentDescription(plusProfile.getPlusAddress());
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private AllPlusAddressesBottomSheetViewBinder() {}
}
