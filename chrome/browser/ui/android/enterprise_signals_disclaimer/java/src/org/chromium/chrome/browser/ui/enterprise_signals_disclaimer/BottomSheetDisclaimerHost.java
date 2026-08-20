// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Implementation of {@link EnterpriseSignalsDisclaimerHost} using {@link BottomSheetController} to
 * display the disclaimer in a bottom sheet.
 */
@NullMarked
class BottomSheetDisclaimerHost implements EnterpriseSignalsDisclaimerHost {
    private final BottomSheetController mBottomSheetController;
    private final EnterpriseSignalsDisclaimerBottomSheetView mSheetContent;
    private boolean mIsActive;

    public BottomSheetDisclaimerHost(
            BottomSheetController bottomSheetController,
            EnterpriseSignalsDisclaimerBottomSheetView sheetContent) {
        mBottomSheetController = bottomSheetController;
        mSheetContent = sheetContent;
        mIsActive = false;
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
        mBottomSheetController.hideContent(mSheetContent, /* animate= */ false);
    }
}
