// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.ON_DISMISSED;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.ON_QUERY_TEXT_CHANGE;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.PLUS_PROFILES;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.QUERY_HINT;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.TITLE;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.WARNING;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.ItemType;
import org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.PlusProfileProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.Locale;

/** Mediator for the all plus addresses bottom sheet UI component. */
class AllPlusAddressesBottomSheetMediator {
    private final PropertyModel mModel;
    private final AllPlusAddressesBottomSheetCoordinator.Delegate mDelegate;
    private @Nullable List<PlusProfile> mProfiles;

    AllPlusAddressesBottomSheetMediator(
            PropertyModel model, AllPlusAddressesBottomSheetCoordinator.Delegate delegate) {
        mDelegate = delegate;
        mModel = model;
    }

    void showPlusProfiles(AllPlusAddressesBottomSheetUIInfo uiInfo) {
        mProfiles = uiInfo.getPlusProfiles();

        mModel.set(TITLE, uiInfo.getTitle());
        mModel.set(WARNING, uiInfo.getWarning());
        mModel.set(QUERY_HINT, uiInfo.getQueryHint());
        mModel.set(ON_QUERY_TEXT_CHANGE, this::onQueryTextChanged);
        mModel.set(ON_DISMISSED, this::onDismissed);

        mModel.get(PLUS_PROFILES).clear();
        for (PlusProfile profile : uiInfo.getPlusProfiles()) {
            final PropertyModel model =
                    PlusProfileProperties.createPlusProfileModel(
                            profile, this::onPlusAddressSelected);
            mModel.get(PLUS_PROFILES).add(new ListItem(ItemType.PLUS_PROFILE, model));
        }
        mModel.set(VISIBLE, true);
    }

    private void onQueryTextChanged(String query) {
        assert mProfiles != null;

        mModel.get(PLUS_PROFILES).clear();
        for (PlusProfile profile : mProfiles) {
            if (!shouldFilter(query.toLowerCase(Locale.ENGLISH), profile)) {
                final PropertyModel model =
                        PlusProfileProperties.createPlusProfileModel(
                                profile, this::onPlusAddressSelected);
                mModel.get(PLUS_PROFILES).add(new ListItem(ItemType.PLUS_PROFILE, model));
            }
        }
    }

    private boolean shouldFilter(String query, PlusProfile profile) {
        return !profile.getPlusAddress().toLowerCase(Locale.ENGLISH).contains(query)
                && !profile.getOrigin().toLowerCase(Locale.ENGLISH).contains(query);
    }

    private void onPlusAddressSelected(String plusAddress) {
        mModel.set(VISIBLE, false);
        mDelegate.onPlusAddressSelected(plusAddress);
    }

    private void onDismissed() {
        mModel.set(VISIBLE, false);
        mDelegate.onDismissed();
    }
}
