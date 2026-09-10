// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerHost.DismissalCause;
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
    private @Nullable Consumer<@DismissalCause Integer> mDismissedCallback;
    private boolean mIsActive;
    private @Nullable @DismissalCause Integer mDismissalCause;

    public BottomSheetDisclaimerHost(
            BottomSheetController bottomSheetController,
            EnterpriseSignalsDisclaimerBottomSheetView sheetContent,
            Consumer<@DismissalCause Integer> onDismissalCallback) {
        mBottomSheetController = bottomSheetController;
        mSheetContent = sheetContent;
        mDismissedCallback = onDismissalCallback;

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
    public void dismiss(@DismissalCause int dismissalCause) {
        mIsActive = false;
        mDismissalCause = dismissalCause;
        mBottomSheetController.hideContent(
                mSheetContent, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
    }

    @Override
    public void destroy() {
        mBottomSheetController.removeObserver(this);
        mSheetContent.setOnDestroyedCallback(null);
        mDismissedCallback = null;
        if (mIsActive) {
            mBottomSheetController.hideContent(mSheetContent, /* animate= */ false);
        }
        mIsActive = false;
    }

    // BottomSheetObserver implementation.
    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        if (mBottomSheetController.getCurrentSheetContent() != mSheetContent) return;

        mIsActive = false;
        if (mDismissedCallback != null) {
            @DismissalCause
            int cause =
                    (mDismissalCause != null)
                            ? mDismissalCause
                            : getDismissalCauseFromStateChangeReason(reason);
            var callback = mDismissedCallback;
            mDismissedCallback = null;
            callback.accept(cause);
        }
    }

    private static @DismissalCause int getDismissalCauseFromStateChangeReason(
            @StateChangeReason int reason) {
        return switch (reason) {
            case StateChangeReason.BACK_PRESS -> DismissalCause.DISMISSED_BY_BACK_PRESS;
            case StateChangeReason.SWIPE -> DismissalCause.DISMISSED_BY_SWIPE_DOWN;
            case StateChangeReason.TAP_SCRIM -> DismissalCause.DISMISSED_BY_TAP_OUTSIDE;
            case StateChangeReason.CLOSE_BUTTON -> DismissalCause.DISMISSED_BY_CLOSE_BUTTON;
            default -> DismissalCause.DISMISSED_WITHOUT_EXPLICIT_USER_ACTION;
        };
    }
}
