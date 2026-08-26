// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

/** Presenter that displays the account picker inside a bottom sheet. */
@NullMarked
class BottomSheetAccountPickerPresenter implements AccountPickerPresenter {
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver;
    private final AccountPickerDismissalLogger mDismissalLogger;
    private final AccountPickerDelegate mAccountPickerDelegate;
    private final Runnable mOnDestroyCallback;
    private @Nullable BottomSheetContent mSheetContent;

    BottomSheetAccountPickerPresenter(
            BottomSheetController bottomSheetController,
            AccountPickerDismissalLogger dismissalLogger,
            AccountPickerDelegate accountPickerDelegate,
            Runnable onDestroyCallback) {
        mBottomSheetController = bottomSheetController;
        mDismissalLogger = dismissalLogger;
        mAccountPickerDelegate = accountPickerDelegate;
        mOnDestroyCallback = onDestroyCallback;

        mBottomSheetObserver =
                new BottomSheetObserver() {
                    @Override
                    public void onSheetStateChanged(
                            @SheetState int newState, @StateChangeReason int reason) {
                        if (newState != BottomSheetController.SheetState.HIDDEN) {
                            return;
                        }
                        mDismissalLogger.logBottomSheetDismissal(reason);
                        if (reason != StateChangeReason.INTERACTION_COMPLETE) {
                            mAccountPickerDelegate.onSignInCancel();
                        }
                        mOnDestroyCallback.run();
                    }
                };
        mBottomSheetController.addObserver(mBottomSheetObserver);
    }

    @Override
    public void show(AccountPickerBottomSheetView view) {
        mSheetContent = view;
        mBottomSheetController.requestShowContent(view, true);
    }

    @Override
    public void dismiss() {
        if (mSheetContent != null) {
            mBottomSheetController.hideContent(
                    mSheetContent, true, StateChangeReason.INTERACTION_COMPLETE);
        }
    }

    @Override
    public void destroy() {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }
}
