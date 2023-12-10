// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import android.app.Activity;

import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.url.GURL;

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
            String modalTitle,
            String plusAddressDescription,
            String proposedPlusAddressPlaceholder,
            String plusAddressModalOkText,
            String plusAddressModalCancelText,
            GURL manageUrl) {
        PlusAddressCreationBottomSheetContent bottomSheetContent =
                new PlusAddressCreationBottomSheetContent(
                        activity,
                        modalTitle,
                        plusAddressDescription,
                        proposedPlusAddressPlaceholder,
                        plusAddressModalOkText,
                        plusAddressModalCancelText,
                        manageUrl);
        mMediator =
                new PlusAddressCreationMediator(
                        bottomSheetContent,
                        bottomSheetController,
                        layoutStateProvider,
                        tabModel,
                        tabModelSelector,
                        bridge);
    }

    public void requestShowContent() {
        mMediator.requestShowContent();
    }

    public void updateProposedPlusAddress(String plusAddress) {
        mMediator.updateProposedPlusAddress(plusAddress);
    }

    public void showError(String message) {
        mMediator.showError(message);
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
