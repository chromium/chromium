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

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.webapps.AppType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The Coordinator for managing the Pwa Universal Install bottom sheet experience. */
@JNINamespace("webapps")
public class PwaUniversalInstallBottomSheetCoordinator {
    public static boolean sEnableManualIconFetching;

    // UniversalInstallDialogActions defined in tools/metrics/histograms/enums.xml
    public static final int DIALOG_SHOWN = 0;
    public static final int INSTALL_APP = 1;
    public static final int OPEN_EXISTING_APP = 2;
    public static final int CREATE_SHORTCUT = 3;
    public static final int CREATE_SHORTCUT_TO_APP = 4;
    // Keep this one at the end and increment appropriately when adding new tasks.
    public static final int DIALOG_RESULT_COUNT = 5;

    private final BottomSheetController mController;
    private final PwaUniversalInstallBottomSheetView mView;
    private final PwaUniversalInstallBottomSheetContent mContent;
    private final PwaUniversalInstallBottomSheetMediator mMediator;
    private @AppType int mAppType = AppType.COUNT;

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
        RecordHistogram.recordEnumeratedHistogram(
                "WebApk.UniversalInstall.DialogAction", DIALOG_SHOWN, DIALOG_RESULT_COUNT);
        return mController.requestShowContent(mContent, /* animate= */ true);
    }

    private void onInstallClicked() {
        RecordHistogram.recordEnumeratedHistogram(
                "WebApk.UniversalInstall.DialogAction", INSTALL_APP, DIALOG_RESULT_COUNT);
        mController.hideContent(mContent, /* animate= */ true);
        mInstallCallback.run();
    }

    private void onAddShortcutClicked() {
        RecordHistogram.recordEnumeratedHistogram(
                "WebApk.UniversalInstall.DialogAction",
                mAppType == AppType.SHORTCUT ? CREATE_SHORTCUT : CREATE_SHORTCUT_TO_APP,
                DIALOG_RESULT_COUNT);
        mController.hideContent(mContent, /* animate= */ true);
        mAddShortcutCallback.run();
    }

    private void onOpenAppClicked() {
        RecordHistogram.recordEnumeratedHistogram(
                "WebApk.UniversalInstall.DialogAction", OPEN_EXISTING_APP, DIALOG_RESULT_COUNT);
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
        RecordHistogram.recordEnumeratedHistogram(
                "WebApk.UniversalInstall.DialogShownForAppType", appType, AppType.COUNT);
        mView.setIcon(icon, adaptive);
        mAppType = appType;

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
