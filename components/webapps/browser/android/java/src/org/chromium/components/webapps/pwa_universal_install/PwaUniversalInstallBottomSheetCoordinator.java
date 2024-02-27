// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_universal_install;

import android.app.Activity;
import android.graphics.Bitmap;
import android.view.View;

import androidx.annotation.MainThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.webapps.AppType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The Coordinator for managing the Pwa Universal Install bottom sheet experience. */
@JNINamespace("webapps")
public class PwaUniversalInstallBottomSheetCoordinator {
    public static boolean sEnableManualIconFetching;

    private final BottomSheetController mController;
    private final PwaUniversalInstallBottomSheetView mView;
    private final PwaUniversalInstallBottomSheetContent mContent;
    private final PwaUniversalInstallBottomSheetMediator mMediator;

    private final Runnable mInstallCallback;
    private final Runnable mAddShortcutCallback;
    private final Runnable mOpenAppCallback;

    /** Constructs the PwaUniversalInstallBottomSheetCoordinator. */
    @MainThread
    public PwaUniversalInstallBottomSheetCoordinator(
            Activity activity,
            WebContents webContents,
            Runnable installCallback,
            Runnable addShortcutCallback,
            Runnable openAppCallback,
            boolean webAppAlreadyInstalled,
            BottomSheetController bottomSheetController,
            int arrowId,
            int installOverlayId,
            int shortcutOverlayId) {
        mController = bottomSheetController;
        mInstallCallback = installCallback;
        mAddShortcutCallback = addShortcutCallback;
        mOpenAppCallback = openAppCallback;

        mView = new PwaUniversalInstallBottomSheetView();
        mView.initialize(activity, webContents, arrowId, installOverlayId, shortcutOverlayId);
        mContent = new PwaUniversalInstallBottomSheetContent(mView);
        mMediator =
                new PwaUniversalInstallBottomSheetMediator(
                        activity,
                        webAppAlreadyInstalled,
                        this::onInstallClicked,
                        this::onAddShortcutClicked,
                        this::onOpenAppClicked);

        PropertyModelChangeProcessor.create(
                mMediator.getModel(), mView, PwaUniversalInstallBottomSheetViewBinder::bind);

        if (!sEnableManualIconFetching) {
            fetchAppData(webContents); // We get a reply back through onAppDataFetched below.
        }
    }

    /**
     * Attempts to show the bottom sheet on the screen.
     *
     * @return True if showing is successful.
     */
    public boolean show() {
        return mController.requestShowContent(mContent, /* animate= */ true);
    }

    private void onInstallClicked() {
        mController.hideContent(mContent, /* animate= */ true);
        mInstallCallback.run();
    }

    private void onAddShortcutClicked() {
        mController.hideContent(mContent, /* animate= */ true);
        mAddShortcutCallback.run();
    }

    private void onOpenAppClicked() {
        mController.hideContent(mContent, /* animate= */ true);
        mOpenAppCallback.run();
    }

    public View getBottomSheetViewForTesting() {
        return mView.getContentView();
    }

    public void fetchAppData(WebContents webContents) {
        PwaUniversalInstallBottomSheetCoordinatorJni.get()
                .fetchAppData(PwaUniversalInstallBottomSheetCoordinator.this, webContents);
    }

    @CalledByNative
    public void onAppDataFetched(@AppType int appType, Bitmap icon, boolean adaptive) {
        mView.setIcon(icon, adaptive);

        boolean alreadyInstalled =
                mMediator.getModel().get(PwaUniversalInstallProperties.VIEW_STATE)
                        == PwaUniversalInstallProperties.ViewState.APP_ALREADY_INSTALLED;
        if (alreadyInstalled) {
            return;
        }

        mMediator
                .getModel()
                .set(
                        PwaUniversalInstallProperties.VIEW_STATE,
                        (appType == AppType.WEBAPK || appType == AppType.WEBAPK_DIY)
                                ? PwaUniversalInstallProperties.ViewState.APP_IS_INSTALLABLE
                                : PwaUniversalInstallProperties.ViewState.APP_IS_NOT_INSTALLABLE);
    }

    @NativeMethods
    interface Natives {
        public void fetchAppData(
                PwaUniversalInstallBottomSheetCoordinator caller, WebContents webContents);
    }
}
