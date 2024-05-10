// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_universal_install;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.SystemClock;
import android.view.View;

import androidx.annotation.MainThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.webapps.AppType;
import org.chromium.components.webapps.R;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.Toast;

/** The Coordinator for managing the Pwa Universal Install bottom sheet experience. */
@JNINamespace("webapps")
public class PwaUniversalInstallBottomSheetCoordinator {
    public static boolean sShowBeforeAppTypeKnownForTesting;
    public static boolean sEnableManualIconFetchingForTesting;

    // UniversalInstallDialogActions defined in tools/metrics/histograms/enums.xml
    public static final int DIALOG_SHOWN = 0;
    public static final int INSTALL_APP = 1;
    public static final int OPEN_EXISTING_APP = 2;
    public static final int CREATE_SHORTCUT = 3;
    public static final int CREATE_SHORTCUT_TO_APP = 4;
    public static final int DIALOG_SHOWN_AFTER_TIMEOUT = 5;
    public static final int DIALOG_CANCELLED_BACK_BUTTON = 6;
    public static final int REDIRECT_TO_SHORTCUT_CREATION = 7;
    public static final int REDIRECT_TO_INSTALL_APP = 8;
    public static final int REDIRECT_TO_INSTALL_APP_DIY = 9;
    // Keep this one at the end and increment appropriately when adding new tasks.
    public static final int DIALOG_RESULT_COUNT = 10;

    // How long (in milliseconds) to wait for the installability check before showing the toast. Set
    // to 100ms because that's about the limit for users to notice that the system isn't reacting
    // instantenously.
    private static final int INITIAL_TOAST_DELAY_MS = 100;
    // How long (in milliseconds) to wait until giving up on waiting for the installability check
    // and showing the bottom sheet. Set to 3.5 seconds to match the duration of the toast we show.
    // NOTE: This may seem long, but this will only take effect for the pathological case, when the
    // installability-check is super slow (e.g. when testing inside the emulator on a heavy site).
    private static final int DIALOG_SHOW_TIMEOUT_MS = 3500;

    private final BottomSheetController mController;
    private final PwaUniversalInstallBottomSheetView mView;
    private final PwaUniversalInstallBottomSheetContent mContent;
    private final PwaUniversalInstallBottomSheetMediator mMediator;

    // Tracks what we're showing this dialog for (specifically, what the results of the
    // installability check was for the site).
    private @AppType Integer mAppType;

    // Whether we are showing the dialog for the root of the domain (path == '/') or a leaf page.
    private boolean mIsRoot;

    // Whether we are yet to show this dialog (the dialog is shown after a brief delay, possibly
    // with a toast while we wait for it to appear).
    private boolean mWaitingToShow = true;

    // The toast to show if the dialog opening takes too long.
    private Toast mToast;

    // Tracks when the fetch application data starts.
    private long mFetchStartTime;

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
        mIsRoot = "/".equals(webContents.getLastCommittedUrl().getPath());

        mView = new PwaUniversalInstallBottomSheetView();
        mView.initialize(activity, webContents, arrowId, installOverlayId, shortcutOverlayId);
        mContent = new PwaUniversalInstallBottomSheetContent(mView, this::recordBackClicked);
        mMediator =
                new PwaUniversalInstallBottomSheetMediator(
                        activity,
                        webAppAlreadyInstalled,
                        this::onInstallClicked,
                        this::onAddShortcutClicked,
                        this::onOpenAppClicked);

        PropertyModelChangeProcessor.create(
                mMediator.getModel(), mView, PwaUniversalInstallBottomSheetViewBinder::bind);

        if (sShowBeforeAppTypeKnownForTesting) {
            show(/* wasTimeout= */ true);
        }

        mFetchStartTime = SystemClock.elapsedRealtime();
        if (!sEnableManualIconFetchingForTesting) {
            fetchAppData(webContents); // We get a reply back through onAppDataFetched below.
        }
    }

    public void showBottomSheetAsync() {
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT, this::showToastIfAppTypeNotKnown, INITIAL_TOAST_DELAY_MS);
    }

    /**
     * Attempts to show the bottom sheet on the screen and fall back to the Install Dialog if
     * showing fails.
     */
    private void show(boolean wasTimeout) {
        mWaitingToShow = false;
        if (mToast != null) mToast.cancel();

        RecordHistogram.recordEnumeratedHistogram(
                "WebApk.UniversalInstall.DialogAction",
                wasTimeout ? DIALOG_SHOWN_AFTER_TIMEOUT : DIALOG_SHOWN,
                DIALOG_RESULT_COUNT);
        if (!mController.requestShowContent(mContent, /* animate= */ true)) {
            // If showing fails without a known app type, fall back to creating shortcut.
            if (mAppType == null) {
                mAddShortcutCallback.run();
                return;
            }

            // App type is known, fall back to appropreate dialog.
            switch (mAppType) {
                case AppType.WEBAPK:
                case AppType.WEBAPK_DIY:
                    mInstallCallback.run();
                    break;
                case AppType.SHORTCUT:
                    mAddShortcutCallback.run();
                    break;
            }
        }
    }

    private void showToastIfAppTypeNotKnown() {
        if (!mWaitingToShow) {
            return;
        }

        if (mAppType == null) {
            mToast =
                    Toast.makeText(
                            ContextUtils.getApplicationContext(),
                            R.string.pwa_uni_install_toast_please_wait_msg,
                            Toast.LENGTH_LONG); // Note: Toast cancels when processing completes.
            mToast.show();
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    this::showDialogIfAppTypeNotKnown,
                    DIALOG_SHOW_TIMEOUT_MS);
        }
    }

    private void showDialogIfAppTypeNotKnown() {
        if (mWaitingToShow) {
            // We've exhausted the wait time, just show the dialog.
            show(/* wasTimeout= */ true);
        }
    }

    private void onInstallClicked() {
        RecordHistogram.recordEnumeratedHistogram(
                "WebApk.UniversalInstall.DialogAction", INSTALL_APP, DIALOG_RESULT_COUNT);
        // It is important to not animate the disappearance of this bottom sheet, because the Rich
        // Install dialog might be shown next, and it (like Universal Install) is also implemented
        // as a bottom sheet. Trying to show and expand one sheet while another is going away
        // doesn't work too well at the moment (the new sheet shows but fails to expand).
        mController.hideContent(mContent, /* animate= */ false);
        mInstallCallback.run();
    }

    private void recordBackClicked() {
        RecordHistogram.recordEnumeratedHistogram(
                "WebApk.UniversalInstall.DialogAction",
                DIALOG_CANCELLED_BACK_BUTTON,
                DIALOG_RESULT_COUNT);
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

    private void logFetchTimeMetrics(@AppType int appType, long fetchDuration) {
        switch (appType) {
            case AppType.WEBAPK:
                RecordHistogram.recordLongTimesHistogram(
                        "WebApk.UniversalInstall.WebApk.AppDataFetchTime", fetchDuration);
                break;
            case AppType.WEBAPK_DIY:
                RecordHistogram.recordLongTimesHistogram(
                        "WebApk.UniversalInstall.Homebrew.AppDataFetchTime", fetchDuration);
                break;
            case AppType.SHORTCUT:
                RecordHistogram.recordLongTimesHistogram(
                        "WebApk.UniversalInstall.Shortcut.AppDataFetchTime", fetchDuration);
                break;
            default:
                assert false;
        }
    }

    @CalledByNative
    public void onAppDataFetched(@AppType int appType, Bitmap icon, boolean adaptive) {
        long fetchDuration = SystemClock.elapsedRealtime() - mFetchStartTime;
        logFetchTimeMetrics(appType, fetchDuration);

        mView.setIcon(icon, adaptive);
        mAppType = appType;
        if (mToast != null) mToast.cancel();

        boolean appAlreadyInstalled =
                mMediator.getModel().get(PwaUniversalInstallProperties.VIEW_STATE)
                        == PwaUniversalInstallProperties.ViewState.APP_ALREADY_INSTALLED;
        if (!appAlreadyInstalled) {
            // This is guarded by checking if the app is installed, because if it is then the dialog
            // is already showing the right information, and we shouldn't change it.
            mMediator
                    .getModel()
                    .set(
                            PwaUniversalInstallProperties.VIEW_STATE,
                            (appType == AppType.WEBAPK || appType == AppType.WEBAPK_DIY)
                                    ? PwaUniversalInstallProperties.ViewState.APP_IS_INSTALLABLE
                                    : PwaUniversalInstallProperties.ViewState
                                            .APP_IS_NOT_INSTALLABLE);
        }

        if (!mWaitingToShow) {
            RecordHistogram.recordEnumeratedHistogram(
                    "WebApk.UniversalInstall.TimeoutWithAppType", appType, AppType.MAX_VALUE);
            // If we are not waiting to show, that means the dialog has shown already while the app
            // type was not known. This allows the metric to catch up to that fact.
            RecordHistogram.recordEnumeratedHistogram(
                    "WebApk.UniversalInstall.DialogShownForAppType", mAppType, AppType.MAX_VALUE);
            return;
        }

        // Beyond this point there are only two outcomes. We'll either show this dialog or redirect
        // to the install dialog. Both outcomes mean that we can stop listening for this flag.
        mWaitingToShow = false;

        // We haven't shown the dialog yet, so there's an opportunity to skip this dialog and
        // redirect straight to the Install App/Create Shortcut dialog.
        if (mAppType == AppType.SHORTCUT
                || (mIsRoot && (mAppType == AppType.WEBAPK || mAppType == AppType.WEBAPK_DIY))) {
            switch (mAppType) {
                case AppType.SHORTCUT:
                    mAddShortcutCallback.run();
                    RecordHistogram.recordEnumeratedHistogram(
                            "WebApk.UniversalInstall.DialogAction",
                            REDIRECT_TO_SHORTCUT_CREATION,
                            DIALOG_RESULT_COUNT);
                    break;
                case AppType.WEBAPK:
                    mInstallCallback.run();
                    RecordHistogram.recordEnumeratedHistogram(
                            "WebApk.UniversalInstall.DialogAction",
                            REDIRECT_TO_INSTALL_APP,
                            DIALOG_RESULT_COUNT);
                    break;
                case AppType.WEBAPK_DIY:
                    mInstallCallback.run();
                    RecordHistogram.recordEnumeratedHistogram(
                            "WebApk.UniversalInstall.DialogAction",
                            REDIRECT_TO_INSTALL_APP_DIY,
                            DIALOG_RESULT_COUNT);
                    break;
                default:
                    assert false;
            }
            return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "WebApk.UniversalInstall.DialogShownForAppType", mAppType, AppType.MAX_VALUE);

        show(/* wasTimeout= */ false);
    }

    @NativeMethods
    interface Natives {
        public void fetchAppData(
                PwaUniversalInstallBottomSheetCoordinator caller, WebContents webContents);
    }
}
