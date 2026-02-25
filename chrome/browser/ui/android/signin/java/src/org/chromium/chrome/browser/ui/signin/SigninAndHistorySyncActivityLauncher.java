// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.Supplier;

/** Allows for launching {@link SigninAndHistorySyncActivity} in modularized code. */
// TODO(https://crbug.com/472425310): Rename this class to SigninCoordinatorFactory.
@NullMarked
public interface SigninAndHistorySyncActivityLauncher {
    /** Sign-in access points that are eligible to the sign-in and history opt-in flow. */
    @IntDef({
        SigninAccessPoint.RECENT_TABS,
        SigninAccessPoint.BOOKMARK_MANAGER,
        SigninAccessPoint.HISTORY_PAGE,
        SigninAccessPoint.NTP_FEED_TOP_PROMO,
        SigninAccessPoint.NTP_FEED_BOTTOM_PROMO,
        SigninAccessPoint.SAFETY_CHECK,
        SigninAccessPoint.SETTINGS,
        SigninAccessPoint.WEB_SIGNIN,
        SigninAccessPoint.NTP_SIGNED_OUT_ICON,
        SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO,
        SigninAccessPoint.SEND_TAB_TO_SELF_PROMO,
        SigninAccessPoint.CCT_ACCOUNT_MISMATCH_NOTIFICATION,
        SigninAccessPoint.COLLABORATION_JOIN_TAB_GROUP,
        SigninAccessPoint.COLLABORATION_SHARE_TAB_GROUP,
        SigninAccessPoint.COLLABORATION_LEAVE_OR_DELETE_TAB_GROUP,
        SigninAccessPoint.HISTORY_SYNC_EDUCATIONAL_TIP,
        SigninAccessPoint.SET_UP_LIST,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface AccessPoint {}

    /**
     * Create {@Intent} for the {@link SigninAndHistorySyncActivity} from an eligible access point,
     * Show an error if the intent can't be created.
     *
     * @param profile the current profile.
     * @param config The object containing configurations for the sign-in & history sync views.
     * @param accessPoint The access point from which the sign-in was triggered.
     */
    @MainThread
    @Nullable Intent createBottomSheetSigninIntentOrShowError(
            Context context,
            Profile profile,
            BottomSheetSigninAndHistorySyncConfig config,
            @AccessPoint int accessPoint);

    /**
     * Creates a coordinator for the bottom-sheet sign-in and history sync flow and registers it to
     * receive activity results using {@link ActivityResultTracker}. Should be called **early** in
     * the embedding UI's creation (e.g. activity onCreate) so the coordinator can receive and
     * handle in-flight activity result if the activity holding the coordinator is killed by the OS.
     * See {@link ActivityResultTracker} for more details.
     *
     * @param windowAndroid The {@link WindowAndroid} for the current window.
     * @param activity The hosting {@link Activity}.
     * @param activityResultTracker The {@link ActivityResultTracker} for launching new activities
     *     and watching for their result.
     * @param delegate The {@link BottomSheetSigninAndHistorySyncCoordinator.Delegate} to be
     *     notified of flow completion for instance.
     * @param deviceLockActivityLauncher The launcher for the device lock challenge.
     * @param profileSupplier The supplier of the {@link Profile}.
     * @param bottomSheetController The {@link BottomSheetController} to show the sign-in bottom
     *     sheet.
     * @param modalDialogManagerSupplier The supplier of the {@link ModalDialogManager}.
     * @param snackbarManager The {@link SnackbarManager} to show sign-in/sign-out snackbars.
     * @param signinAccessPoint The entry point for the sign-in flow.
     */
    @MainThread
    BottomSheetSigninAndHistorySyncCoordinator
            createBottomSheetSigninCoordinatorAndObserveAddAccountResult(
                    WindowAndroid windowAndroid,
                    Activity activity,
                    ActivityResultTracker activityResultTracker,
                    BottomSheetSigninAndHistorySyncCoordinator.Delegate delegate,
                    DeviceLockActivityLauncher deviceLockActivityLauncher,
                    OneshotSupplier<Profile> profileSupplier,
                    Supplier<BottomSheetController> bottomSheetController,
                    Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
                    SnackbarManager snackbarManager,
                    @SigninAccessPoint int signinAccessPoint);

    /**
     * Create {@Intent} for the fullscreen flavor of the {@link SigninAndHistorySyncActivity} if
     * sign-in and history opt-in are allowed. Does not show any error if the intent can't be
     * created.
     *
     * @param config The object containing IDS of resources for the sign-in & history sync views.
     * @param accessPoint The access point from which the sign-in was triggered.
     */
    @MainThread
    @Nullable Intent createFullscreenSigninIntent(
            Context context,
            Profile profile,
            FullscreenSigninAndHistorySyncConfig config,
            @SigninAccessPoint int signinAccessPoint);

    /**
     * Create {@Intent} for the fullscreen flavor of the {@link SigninAndHistorySyncActivity} if
     * sign-in and history opt-in are allowed. Show an error if the intent can't be created.
     *
     * @param config The object containing IDS of resources for the sign-in & history sync views.
     * @param accessPoint The access point from which the sign-in was triggered.
     */
    @MainThread
    @Nullable
    Intent createFullscreenSigninIntentOrShowError(
            Context context,
            Profile profile,
            FullscreenSigninAndHistorySyncConfig config,
            @SigninAccessPoint int signinAccessPoint);
}
