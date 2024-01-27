// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_universal_install;

import android.app.Activity;
import android.view.View;

import androidx.annotation.MainThread;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The Coordinator for managing the Pwa Universal Install bottom sheet experience. */
public class PwaUniversalInstallBottomSheetCoordinator {
    private final BottomSheetController mController;
    private final PwaUniversalInstallBottomSheetView mView;
    private final PwaUniversalInstallBottomSheetContent mContent;
    private final PwaUniversalInstallBottomSheetMediator mMediator;

    /** Constructs the PwaUniversalInstallBottomSheetCoordinator. */
    @MainThread
    public PwaUniversalInstallBottomSheetCoordinator(
            Activity activity, BottomSheetController bottomSheetController, int arrowId) {
        mController = bottomSheetController;

        mView = new PwaUniversalInstallBottomSheetView(activity);
        mView.initialize(arrowId);
        mContent = new PwaUniversalInstallBottomSheetContent(mView);
        mMediator = new PwaUniversalInstallBottomSheetMediator(activity);

        PropertyModelChangeProcessor.create(
                mMediator.getModel(), mView, PwaUniversalInstallBottomSheetViewBinder::bind);
    }

    /**
     * Attempts to show the bottom sheet on the screen.
     *
     * @return True if showing is successful.
     */
    public boolean show() {
        return mController.requestShowContent(mContent, true);
    }

    public View getBottomSheetViewForTesting() {
        return mView.getContentView();
    }
}
