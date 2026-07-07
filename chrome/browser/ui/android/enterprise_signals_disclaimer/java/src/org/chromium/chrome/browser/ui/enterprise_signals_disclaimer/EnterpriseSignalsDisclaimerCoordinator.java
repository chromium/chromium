// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the enterprise signals disclaimer bottom sheet. Manages the lifecycle and display
 * of the disclaimer content.
 */
@NullMarked
public class EnterpriseSignalsDisclaimerCoordinator {
    private final BottomSheetController mBottomSheetController;
    private final EnterpriseSignalsDisclaimerBottomSheetView mSheetContent;
    private final EnterpriseSignalsDisclaimerMediator mMediator;
    private final PropertyModelChangeProcessor mModelChangeProcessor;

    /**
     * Constructs an {@link EnterpriseSignalsDisclaimerCoordinator}.
     *
     * @param context The Android {@link Context}.
     * @param bottomSheetController The {@link BottomSheetController} for showing the bottom sheet.
     */
    public EnterpriseSignalsDisclaimerCoordinator(
            Context context, BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
        mSheetContent = new EnterpriseSignalsDisclaimerBottomSheetView(context);
        mMediator = new EnterpriseSignalsDisclaimerMediator(context);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(),
                        mSheetContent,
                        EnterpriseSignalsDisclaimerViewBinder::bind);
    }

    /** Shows the enterprise signals disclaimer bottom sheet. */
    public void show() {
        mBottomSheetController.requestShowContent(mSheetContent, /* animate= */ true);
    }

    /** Destroys the coordinator, hiding the sheet and cleaning up resources. */
    public void destroy() {
        mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
        mModelChangeProcessor.destroy();
    }
}
