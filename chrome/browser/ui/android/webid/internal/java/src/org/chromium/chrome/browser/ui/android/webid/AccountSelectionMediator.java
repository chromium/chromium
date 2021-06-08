// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.List;

/**
 * Contains the logic for the AccountSelection component. It sets the state of the model and reacts
 * to events like clicks.
 */
class AccountSelectionMediator {
    private boolean mVisible;
    private final AccountSelectionComponent.Delegate mDelegate;
    private final ModelList mSheetItems;

    private final BottomSheetController mBottomSheetController;
    private final BottomSheetContent mBottomSheetContent;
    private final BottomSheetObserver mBottomSheetObserver;

    AccountSelectionMediator(AccountSelectionComponent.Delegate delegate, ModelList sheetItems,
            BottomSheetController bottomSheetController, BottomSheetContent bottomSheetContent) {
        assert delegate != null;
        mVisible = false;
        mDelegate = delegate;
        mSheetItems = sheetItems;
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;

        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            // TODO(majidvp): We should override #onSheetStateChanged() and react to HIDDEN state
            // since closed is a legacy fixture that can get out of sync with the state is some
            // situations. https://crbug.com/1215174
            @Override
            public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                super.onSheetClosed(reason);
                mBottomSheetController.removeObserver(mBottomSheetObserver);

                if (!mVisible) return;
                onDismissed(reason);
            }
        };
    }

    void showAccounts(String url, List<Account> accounts) {
        // TODO (majidvp): Update mSheetItems and show the view.
        showContent();
    }

    /**
     * Requests to show the bottom sheet. If it is not possible to immediately show the content
     * (e.g., higher priority content is being shown) it removes the request from the bottom sheet
     * controller queue and notifies the delegate of the dismissal.
     */
    @VisibleForTesting
    void showContent() {
        if (mBottomSheetController.requestShowContent(mBottomSheetContent, true)) {
            mVisible = true;
            mBottomSheetController.addObserver(mBottomSheetObserver);
        } else {
            onDismissed(BottomSheetController.StateChangeReason.NONE);
        }
    }

    /**
     * Requests to hide the bottom sheet.
     */
    void hideContent() {
        mVisible = false;
        mBottomSheetController.hideContent(mBottomSheetContent, true);
    }

    boolean isVisible() {
        return mVisible;
    }

    void onAccountSelected(Account account) {
        hideContent();
        mDelegate.onAccountSelected(account);
    }

    void onDismissed(@StateChangeReason int reason) {
        hideContent();
        mDelegate.onDismissed();
    }
}
