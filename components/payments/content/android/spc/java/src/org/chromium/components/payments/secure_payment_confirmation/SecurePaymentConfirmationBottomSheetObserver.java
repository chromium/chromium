// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import static org.chromium.build.NullUtil.assertNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;

/** The secure payment confirmation implementation of the bottoms sheet observer. */
@NullMarked
/*package*/ class SecurePaymentConfirmationBottomSheetObserver extends EmptyBottomSheetObserver {
    /** Controller callbacks for secure payment confirmation. */
    interface ControllerDelegate {
        void onCancel();
    }

    private final BottomSheetController mBottomSheetController;
    private @Nullable ControllerDelegate mDelegate;
    private boolean mFinished;

    /**
     * Constructs the lifecycle for the save card bottom sheet.
     *
     * @param bottomSheetController The controller to use for showing or hiding the content.
     */
    SecurePaymentConfirmationBottomSheetObserver(BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
    }

    /**
     * Begins the lifecycle of the secure payment confirmation bottom sheet. Starts observing tab
     * and layout changes.
     *
     * @param delegate The controller callbacks for user actions.
     */
    void begin(ControllerDelegate delegate) {
        assert mDelegate == null;
        mDelegate = delegate;
        mBottomSheetController.addObserver(this);
    }

    /** Ends the lifecycle of the secure payment confirmation bottom sheet. */
    void end() {
        mBottomSheetController.removeObserver(this);
        mDelegate = null;
        mFinished = false;
    }

    // Overrides EmptyBottomSheetObserver onSheetClosed method for BottomSheetController.
    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        if (reason == StateChangeReason.INTERACTION_COMPLETE) {
            mFinished = true;
        } else if (!mFinished) {
            mFinished = true;
            assertNonNull(mDelegate);
            mDelegate.onCancel();
        }
    }
}
