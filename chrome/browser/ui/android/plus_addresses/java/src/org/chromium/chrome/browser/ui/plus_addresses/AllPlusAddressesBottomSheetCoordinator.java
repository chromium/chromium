// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.ItemType.PLUS_PROFILE;
import static org.chromium.chrome.browser.ui.plus_addresses.AllPlusAddressesBottomSheetProperties.PLUS_PROFILES;

import android.content.Context;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.autofill.helpers.FaviconHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * Coordinator for the all plus addresses bottom sheet UI component.
 *
 * <p>This component shows a bottom sheet to let the user select a plus address from the list of all
 * user's plus addresses.
 *
 * <p>This coordinator manages the lifecycle of the bottom sheet mediator and view.
 */
class AllPlusAddressesBottomSheetCoordinator {
    private final AllPlusAddressesBottomSheetMediator mMeditor;

    /**
     * This delegate is called when the AllPlusAddressesBottomSheet is interacted with (e.g.
     * dismissed or a suggestion was selected).
     */
    static interface Delegate {
        /**
         * Called when the user taps on one of the plus addresses chips.
         *
         * @param plusAddress The main text of the selected chip view.
         */
        void onPlusAddressSelected(String plusAddress);

        /**
         * Called when the user dismisses the AllPlusAddressesBottomSheet or if the bottom sheet
         * content failed to be shown.
         */
        void onDismissed();
    }

    AllPlusAddressesBottomSheetCoordinator(
            Context context,
            BottomSheetController sheetController,
            Delegate delegate,
            FaviconHelper helper) {
        PropertyModel model = AllPlusAddressesBottomSheetProperties.createDefaultModel();
        mMeditor = new AllPlusAddressesBottomSheetMediator(model, delegate);
        AllPlusAddressesBottomSheetView view =
                new AllPlusAddressesBottomSheetView(context, sheetController);

        view.setSheetItemListAdapter(createSheetItemListAdapter(model.get(PLUS_PROFILES), helper));
        PropertyModelChangeProcessor.create(
                model,
                view,
                AllPlusAddressesBottomSheetViewBinder::bindAllPlusAddressesBottomSheet);
    }

    @VisibleForTesting
    static RecyclerView.Adapter createSheetItemListAdapter(
            ModelList profiles, FaviconHelper helper) {
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(profiles);
        adapter.registerType(
                PLUS_PROFILE,
                AllPlusAddressesBottomSheetViewBinder::createPlusAddressView,
                (model, view, key) ->
                        AllPlusAddressesBottomSheetViewBinder.bindPlusAddressView(
                                model, view, key, helper));
        return adapter;
    }

    void showPlusProfiles(AllPlusAddressesBottomSheetUIInfo uiInfo) {
        mMeditor.showPlusProfiles(uiInfo);
    }
}
