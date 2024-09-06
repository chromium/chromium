// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator of the plus address creation UI. */
public class PlusAddressCreationCoordinator {
    private PlusAddressCreationMediator mMediator;

    public PlusAddressCreationCoordinator(
            Activity activity,
            BottomSheetController bottomSheetController,
            LayoutStateProvider layoutStateProvider,
            TabModel tabModel,
            TabModelSelector tabModelSelector,
            PlusAddressCreationViewBridge bridge,
            PlusAddressCreationNormalStateInfo info,
            boolean refreshSupported) {
        PropertyModel model = PlusAddressCreationProperties.createDefaultModel();
        PlusAddressCreationBottomSheetContent bottomSheetContent =
                new PlusAddressCreationBottomSheetContent(
                        activity, bottomSheetController, info, refreshSupported);
        mMediator =
                new PlusAddressCreationMediator(
                        model,
                        bottomSheetContent,
                        bottomSheetController,
                        layoutStateProvider,
                        tabModel,
                        tabModelSelector,
                        bridge);

        PropertyModelChangeProcessor.create(
                model,
                bottomSheetContent,
                PlusAddressCreationViewBinder::bindPlusAddressCreationBottomSheet);
    }

    public void requestShowContent() {
        mMediator.requestShowContent();
    }

    public void updateProposedPlusAddress(String plusAddress) {
        mMediator.updateProposedPlusAddress(plusAddress);
    }

    public void showError(@Nullable PlusAddressCreationErrorStateInfo errorStateInfo) {
        mMediator.showError(errorStateInfo);
    }

    public void hideRefreshButton() {
        mMediator.hideRefreshButton();
    }

    public void finishConfirm() {
        mMediator.onConfirmFinished();
    }

    public void destroy() {
        mMediator.destroy();
    }

    public void setMediatorForTesting(PlusAddressCreationMediator mediator) {
        mMediator = mediator;
    }
}
