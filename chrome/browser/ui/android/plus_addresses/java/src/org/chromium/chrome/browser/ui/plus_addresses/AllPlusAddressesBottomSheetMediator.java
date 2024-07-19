// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.PLUS_PROFILES;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.VISIBLE;

import org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.ItemType;
import org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.PlusProfileProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the all plus addresses bottom sheet UI component. */
class AllPlusAddressesBottomSheetMediator {
    private final PropertyModel mModel;

    AllPlusAddressesBottomSheetMediator(PropertyModel model) {
        mModel = model;
    }

    void showPlusProfiles(AllPlusAddressesBottomSheetUIInfo uiInfo) {
        mModel.get(PLUS_PROFILES).clear();
        for (PlusProfile profile : uiInfo.getPlusProfiles()) {
            final PropertyModel model = PlusProfileProperties.createPlusProfileModel(profile);
            mModel.get(PLUS_PROFILES).add(new ListItem(ItemType.PLUS_PROFILE, model));
        }
        mModel.set(VISIBLE, true);
    }
}
