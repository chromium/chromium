// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.app.Activity;
import android.graphics.Bitmap;
import android.view.View;

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.components.webapps.pwa_restore_ui.PwaRestoreProperties.ViewState;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/** The Coordinator for managing the Pwa Restore bottom sheet experience. */
public class PwaRestoreBottomSheetCoordinator implements BottomSheetObserver {
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
        // The controller can be null in render tests.
        if (mController != null) {
            mController.addObserver(this);
        }

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

    // Interface BottomSheetObserver:

    @Override
    public void onSheetOpened(@StateChangeReason int reason) {}

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {}

    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {}

    @Override
    public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {}

    @Override
    public void onSheetStateChanged(@SheetState int newState, @StateChangeReason int reason) {
        // By default, a scrim isn't provided for a Bottom Sheet while in peeking mode. This creates
        // problems (see bug), especially since our dialog is a bit large due to the illustration
        // icon. A scrim is therefore required, so we provide it manually (when in peeking mode).
        if (newState == BottomSheetController.SheetState.PEEK) {
            mController.getScrimCoordinator().showScrim(createScrimPropertyModel());
        }
    }

    private PropertyModel createScrimPropertyModel() {
        PropertyModel scrimModel =
                new PropertyModel.Builder(ScrimProperties.REQUIRED_KEYS)
                        .with(ScrimProperties.TOP_MARGIN, 0)
                        .with(ScrimProperties.AFFECTS_STATUS_BAR, true)
                        .with(ScrimProperties.ANCHOR_VIEW, mView.getContentView())
                        .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, false)
                        .with(
                                ScrimProperties.CLICK_DELEGATE,
                                () -> {
                                    hideBottomSheet();
                                })
                        .build();
        return scrimModel;
    }

    // Hides the bottom sheet and our custom scrim (partially translucent overlay) for the preview
    // mode (aka. peek state).
    private void hideBottomSheet() {
        mController.getScrimCoordinator().hideScrim(/* animate= */ true);
        mController.hideContent(mContent, /* animate= */ true);
        mController.removeObserver(this);
    }

    protected void onReviewButtonClicked() {
        mMediator.setPreviewState();
        mController.expandSheet();
    }

    protected void onDialogBackButtonClicked() {
        mMediator.setPeekingState();
        mController.collapseSheet(/* animate= */ true);
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
        mController.hideContent(mContent, /* animate= */ true);
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
