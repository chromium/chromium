// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_universal_install;

import android.app.Activity;
import android.graphics.Bitmap;
import android.util.Pair;
import android.view.View;

import androidx.annotation.MainThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.concurrent.Callable;

/** The Coordinator for managing the Pwa Universal Install bottom sheet experience. */
@JNINamespace("webapps")
public class PwaUniversalInstallBottomSheetCoordinator {
    // If set, this swaps out the icon fetching callback for testing.
    private static Callable<Pair<Bitmap, Boolean>> sIconCall;

    public static void setIconCallForTesting(Callable<Pair<Bitmap, Boolean>> iconCall) {
        sIconCall = iconCall;
    }

    private final BottomSheetController mController;
    private final PwaUniversalInstallBottomSheetView mView;
    private final PwaUniversalInstallBottomSheetContent mContent;
    private final PwaUniversalInstallBottomSheetMediator mMediator;
    private final WebContents mWebContents;

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
            int arrowId) {
        mWebContents = webContents;
        mController = bottomSheetController;
        mInstallCallback = installCallback;
        mAddShortcutCallback = addShortcutCallback;
        mOpenAppCallback = openAppCallback;

        mView = new PwaUniversalInstallBottomSheetView();
        mView.initialize(
                activity, webContents, sIconCall != null ? sIconCall : this::getIcon, arrowId);
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
    }

    /**
     * Attempts to show the bottom sheet on the screen.
     *
     * @return True if showing is successful.
     */
    public boolean show() {
        return mController.requestShowContent(mContent, /* animate= */ true);
    }

    /**
     * This function starts the icon fetching asynchronously and returns null to signify that the
     * icons are not yet available. This is overwritten in tests to return actual icon data
     * synchronously.
     */
    private Pair<Bitmap, Boolean> getIcon() {
        fetchAppIcon(mWebContents); // We get a reply back through onAppIconFetched below.
        return null;
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

    public void fetchAppIcon(WebContents webContents) {
        PwaUniversalInstallBottomSheetCoordinatorJni.get()
                .fetchAppIcon(PwaUniversalInstallBottomSheetCoordinator.this, webContents);
    }

    @CalledByNative
    public void onAppIconFetched(Bitmap icon, boolean adaptive) {
        mView.setIcon(icon, adaptive);
    }

    @NativeMethods
    interface Natives {
        public void fetchAppIcon(
                PwaUniversalInstallBottomSheetCoordinator caller, WebContents webContents);
    }
}
