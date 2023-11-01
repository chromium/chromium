// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import android.app.Activity;

import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Coordinator of the plus address creation UI. */
public class PlusAddressCreationCoordinator {
    private PlusAddressCreationMediator mMediator;

    public PlusAddressCreationCoordinator(
            Activity activity,
            BottomSheetController bottomSheetController,
            LayoutStateProvider layoutStateProvider,
            TabModel tabModel,
            PlusAddressCreationViewBridge bridge,
            String modalTitle,
            String plusAddressDescription,
            String proposedPlusAddressPlaceholder,
            String plusAddressModalOkText,
            String plusAddressModalCancelText) {
        PlusAddressCreationBottomSheetContent bottomSheetContent =
                new PlusAddressCreationBottomSheetContent(
                        activity,
                        modalTitle,
                        plusAddressDescription,
                        proposedPlusAddressPlaceholder,
                        plusAddressModalOkText,
                        plusAddressModalCancelText);
        mMediator =
                new PlusAddressCreationMediator(
                        bottomSheetContent,
                        bottomSheetController,
                        layoutStateProvider,
                        tabModel,
                        bridge);
    }

    public void requestShowContent() {
        mMediator.requestShowContent();
    }

    public void destroy() {
        mMediator.destroy();
    }

    public void setMediatorForTesting(PlusAddressCreationMediator mediator) {
        mMediator = mediator;
    }
}
