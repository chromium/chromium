// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.app.Activity;
import android.graphics.Bitmap;
import android.view.View;

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.webapps.pwa_restore_ui.PwaRestoreProperties.ViewState;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/** The Coordinator for managing the Pwa Restore bottom sheet experience. */
public class PwaRestoreBottomSheetCoordinator {
    private final BottomSheetController mController;
    private final PwaRestoreBottomSheetView mView;
    private final PwaRestoreBottomSheetContent mContent;
    private final PwaRestoreBottomSheetMediator mMediator;

    /** Constructs the PwaRestoreBottomSheetCoordinator. */
    @MainThread
    public PwaRestoreBottomSheetCoordinator(
            @NonNull String[] appIds,
            @NonNull String[] appNames,
            @NonNull List<Bitmap> appIcons,
            @NonNull int[] lastUsedInDays,
            Activity activity,
            BottomSheetController bottomSheetController,
            int backArrowId) {
        mController = bottomSheetController;

        ArrayList<PwaRestoreProperties.AppInfo> apps = new ArrayList();

        assert appIds.length == appNames.length;
        assert appIds.length == lastUsedInDays.length;
        assert appIds.length == appIcons.size();
        for (int i = 0; i < appIds.length; i++) {
            apps.add(
                    new PwaRestoreProperties.AppInfo(
                            appIds[i], appNames[i], appIcons.get(i), lastUsedInDays[i]));
        }

        mView = new PwaRestoreBottomSheetView(activity);
        mView.initialize(backArrowId);
        mContent = new PwaRestoreBottomSheetContent(mView, this::onOsBackButtonClicked);
        mMediator =
                new PwaRestoreBottomSheetMediator(
                        apps,
                        activity,
                        this::onReviewButtonClicked,
                        this::onRestoreButtonClicked,
                        this::onDialogBackButtonClicked);

        PropertyModelChangeProcessor.create(
                mMediator.getModel(), mView, PwaRestoreBottomSheetViewBinder::bind);
    }

    /**
     * Attempts to show the bottom sheet on the screen.
     *
     * @return True if showing is successful.
     */
    public boolean show() {
        return mController.requestShowContent(mContent, true);
    }

    private void hideBottomSheet() {
        mController.hideContent(mContent, /* animate= */ true);
    }

    protected void onReviewButtonClicked() {
        mMediator.setPreviewState();
    }

    protected void onDialogBackButtonClicked() {
        mMediator.setPeekingState();
    }

    protected void onOsBackButtonClicked() {
        if (mMediator.getModel().get(PwaRestoreProperties.VIEW_STATE) == ViewState.VIEW_PWA_LIST) {
            // When the Android Back button is pressed while showing the PWA list, we should go back
            // to the initial stage (essentially do what the Back button on the dialog does).
            onDialogBackButtonClicked();
        } else {
            // If we are already in initial stage, we should just close the dialog.
            hideBottomSheet();
        }
    }

    protected void onRestoreButtonClicked() {
        hideBottomSheet();
    }

    protected PropertyModel getModelForTesting() {
        return mMediator.getModel();
    }

    protected View getBottomSheetViewForTesting() {
        return mView.getContentView();
    }
}
