// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.app.Activity;
import android.view.View;

import androidx.annotation.MainThread;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The Coordinator for managing the Pwa Restore bottom sheet experience.
 */
public class PwaRestoreBottomSheetCoordinator {
    private final BottomSheetController mController;
    private final PwaRestoreBottomSheetView mView;
    private final PwaRestoreBottomSheetContent mContent;
    private final PwaRestoreBottomSheetMediator mMediator;

    /**
     * Constructs the PwaRestoreBottomSheetCoordinator.
     */
    @MainThread
    public PwaRestoreBottomSheetCoordinator(
            Activity activity, BottomSheetController bottomSheetController, int backArrowId) {
        mController = bottomSheetController;

        mView = new PwaRestoreBottomSheetView(activity);
        mView.initialize(backArrowId);
        mContent = new PwaRestoreBottomSheetContent(mView);
        mMediator = new PwaRestoreBottomSheetMediator(
                activity, this::onReviewButtonClicked, this::onBackButtonClicked);

        PropertyModelChangeProcessor.create(
                mMediator.getModel(), mView, PwaRestoreBottomSheetViewBinder::bind);
    }

    /**
     * Attempts to show the bottom sheet on the screen.
     * @return True if showing is successful.
     */
    public boolean show() {
        return mController.requestShowContent(mContent, true);
    }

    protected void onReviewButtonClicked() {
        mMediator.setPreviewState();
        mController.expandSheet();
    }

    protected void onBackButtonClicked() {
        mMediator.setPeekingState();
        mController.collapseSheet(/* animate= */ true);
    }

    protected PropertyModel getModelForTesting() {
        return mMediator.getModel();
    }

    protected View getBottomSheetToolbarViewForTesting() {
        return mView.getPreviewView();
    }

    protected View getBottomSheetContentViewForTesting() {
        return mView.getContentView();
    }
}
