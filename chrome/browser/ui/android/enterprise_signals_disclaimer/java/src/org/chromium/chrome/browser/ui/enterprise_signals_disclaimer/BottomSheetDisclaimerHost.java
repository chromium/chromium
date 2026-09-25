// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import android.content.res.Resources;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerHost.DismissalCause;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

import java.util.function.Consumer;
import java.util.function.IntSupplier;

/**
 * Implementation of {@link EnterpriseSignalsDisclaimerHost} using {@link BottomSheetController} to
 * display the disclaimer in a bottom sheet.
 */
@NullMarked
class BottomSheetDisclaimerHost implements EnterpriseSignalsDisclaimerHost, BottomSheetObserver {
    private final BottomSheetController mBottomSheetController;
    private final SheetContent mSheetContent;
    private @Nullable Consumer<@DismissalCause Integer> mDismissedCallback;
    private boolean mIsActive;
    private @Nullable @DismissalCause Integer mDismissalCause;

    /**
     * @param bottomSheetController The {@link BottomSheetController} used to show the sheet.
     * @param contentView The disclaimer view to host.
     * @param verticalScrollOffsetSupplier Supplies the vertical scroll offset of the content.
     * @param onDismissalCallback Invoked with the {@link DismissalCause} once the sheet is closed.
     */
    public BottomSheetDisclaimerHost(
            BottomSheetController bottomSheetController,
            View contentView,
            IntSupplier verticalScrollOffsetSupplier,
            Consumer<@DismissalCause Integer> onDismissalCallback) {
        mBottomSheetController = bottomSheetController;
        mSheetContent = new SheetContent(contentView, verticalScrollOffsetSupplier);
        mDismissedCallback = onDismissalCallback;

        mBottomSheetController.addObserver(this);
    }

    // EnterpriseSignalsDisclaimerHost implementation.
    @Override
    public void show() {
        mIsActive = true;
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

    /** {@link BottomSheetContent} wrapping the disclaimer view. */
    private class SheetContent implements BottomSheetContent {
        private final View mContentView;
        private final IntSupplier mVerticalScrollOffsetSupplier;

        SheetContent(View contentView, IntSupplier verticalScrollOffsetSupplier) {
            mContentView = contentView;
            mVerticalScrollOffsetSupplier = verticalScrollOffsetSupplier;
        }

        @Override
        public View getContentView() {
            return mContentView;
        }

        @Override
        public @Nullable View getToolbarView() {
            return null;
        }

        @Override
        public int getVerticalScrollOffset() {
            return mVerticalScrollOffsetSupplier.getAsInt();
        }

        @Override
        public void destroy() {
            // The sheet content was destroyed by the BottomSheetController, e.g. because it was
            // dropped from the queue. It will not be shown anymore.
            mIsActive = false;
        }

        @Override
        public int getPriority() {
            return BottomSheetContent.ContentPriority.HIGH;
        }

        @Override
        public boolean swipeToDismissEnabled() {
            return true;
        }

        @Override
        public boolean showHandlebar() {
            return true;
        }

        @Override
        public float getFullHeightRatio() {
            return BottomSheetContent.HeightMode.WRAP_CONTENT;
        }

        @Override
        public @StringRes int getSheetHalfHeightAccessibilityStringId() {
            // Half height is not supported.
            return Resources.ID_NULL;
        }

        @Override
        public @StringRes int getSheetFullHeightAccessibilityStringId() {
            return R.string.enterprise_signals_disclaimer_sheet_full_height;
        }

        @Override
        public @StringRes int getSheetClosedAccessibilityStringId() {
            return R.string.enterprise_signals_disclaimer_sheet_closed;
        }
    }
}
