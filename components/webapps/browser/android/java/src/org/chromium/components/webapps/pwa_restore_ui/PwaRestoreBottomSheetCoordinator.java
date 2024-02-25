// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.app.Activity;
import android.view.View;

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;

/** The Coordinator for managing the Pwa Restore bottom sheet experience. */
public class PwaRestoreBottomSheetCoordinator {
    private final BottomSheetController mController;
    private final PwaRestoreBottomSheetView mView;
    private final PwaRestoreBottomSheetContent mContent;
    private final PwaRestoreBottomSheetMediator mMediator;

    // How long an app can go unused and still be considered 'recent' in terms
    // of the restore UI.
    private static final int CUTOFF_FOR_OLDER_APPS_IN_DAYS = 30;

    /** Constructs the PwaRestoreBottomSheetCoordinator. */
    @MainThread
    public PwaRestoreBottomSheetCoordinator(
            @NonNull String[][] appList,
            @NonNull int[] lastUsedInDays,
            Activity activity,
            BottomSheetController bottomSheetController,
            int backArrowId) {
        mController = bottomSheetController;

        ArrayList<PwaRestoreProperties.AppInfo> recentApps = new ArrayList();
        ArrayList<PwaRestoreProperties.AppInfo> olderApps = new ArrayList();
        int i = 0;
        for (String[] app : appList) {
            if (lastUsedInDays[i] < CUTOFF_FOR_OLDER_APPS_IN_DAYS) {
                recentApps.add(new PwaRestoreProperties.AppInfo(app[0], app[1], lastUsedInDays[i]));
            } else {
                olderApps.add(new PwaRestoreProperties.AppInfo(app[0], app[1], lastUsedInDays[i]));
            }
            i++;
        }

        mView = new PwaRestoreBottomSheetView(activity);
        mView.initialize(backArrowId);
        mContent = new PwaRestoreBottomSheetContent(mView);
        mMediator =
                new PwaRestoreBottomSheetMediator(
                        recentApps,
                        olderApps,
                        activity,
                        this::onReviewButtonClicked,
                        this::onBackButtonClicked);

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
