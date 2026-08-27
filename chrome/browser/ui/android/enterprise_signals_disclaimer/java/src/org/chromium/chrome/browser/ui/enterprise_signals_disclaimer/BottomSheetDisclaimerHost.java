// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

import java.util.function.Consumer;

/**
 * Implementation of {@link EnterpriseSignalsDisclaimerHost} using {@link BottomSheetController} to
 * display the disclaimer in a bottom sheet.
 */
@NullMarked
class BottomSheetDisclaimerHost implements EnterpriseSignalsDisclaimerHost, BottomSheetObserver {
    private final BottomSheetController mBottomSheetController;
    private final EnterpriseSignalsDisclaimerBottomSheetView mSheetContent;
    private @Nullable Consumer<Boolean> mSheetDismissedCallback;
    private boolean mIsActive;

    public BottomSheetDisclaimerHost(
            BottomSheetController bottomSheetController,
            EnterpriseSignalsDisclaimerBottomSheetView sheetContent,
            Consumer<Boolean> sheetDismissedCallback) {
        mBottomSheetController = bottomSheetController;
        mSheetContent = sheetContent;
        mSheetDismissedCallback = sheetDismissedCallback;

        mBottomSheetController.addObserver(this);
    }

    // EnterpriseSignalsDisclaimerHost implementation.
    @Override
    public void show() {
        mIsActive = true;
        mSheetContent.setOnDestroyedCallback(() -> mIsActive = false);
        mBottomSheetController.requestShowContent(mSheetContent, /* animate= */ true);
    }

    @Override
    public boolean isActive() {
        return mIsActive;
    }

    @Override
    public void hide() {
        mIsActive = false;
        mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
    }

    @Override
    public void destroy() {
        mBottomSheetController.removeObserver(this);
        mSheetContent.setOnDestroyedCallback(null);
        mSheetDismissedCallback = null;
        mIsActive = false;
        mBottomSheetController.hideContent(mSheetContent, /* animate= */ false);
    }

    // BottomSheetObserver implementation.
    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        if (mBottomSheetController.getCurrentSheetContent() != mSheetContent) return;

        mIsActive = false;
        if (mSheetDismissedCallback != null) {
            boolean isUserAction =
                    reason == StateChangeReason.SWIPE
                            || reason == StateChangeReason.BACK_PRESS
                            || reason == StateChangeReason.TAP_SCRIM
                            || reason == StateChangeReason.CLOSE_BUTTON;
            mSheetDismissedCallback.accept(isUserAction);
            mSheetDismissedCallback = null;
        }
    }
}
