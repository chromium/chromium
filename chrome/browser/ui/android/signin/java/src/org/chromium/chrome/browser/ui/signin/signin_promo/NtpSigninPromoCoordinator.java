// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.app.Activity;
import android.view.View;
import android.view.ViewStub;

import org.chromium.base.supplier.SupplierUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/** Coordinator for the seamless sign-in promo card in NTP. */
@NullMarked
public class NtpSigninPromoCoordinator {
    private final SigninPromoCoordinator mSigninPromoCoordinator;
    private final ViewStub mSigninPromoViewContainerStub;
    private @Nullable PersonalizedSigninPromoView mSigninPromoView;

    /**
     * Creates an instance of the {@link NtpSigninPromoCoordinator}.
     *
     * @param windowAndroid The window showing this recent tabs page.
     * @param activity The Android Activity this manager will work in.
     * @param profile A {@link Profile} object to access identity services. This must be the
     *     original profile, not the incognito one.
     * @param activityResultTracker Tracker of activity results.
     * @param launcher A {@SigninAndHistorySyncActivityLauncher} for the initialization of {@link
     *     SigninPromoDelegate}.
     * @param bottomSheetController Used to interact with the bottom sheet.
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager}.
     * @param snackbarManager Manages snackbars shown in the app.
     * @param deviceLockActivityLauncher The launcher to start up the device lock page.
     * @param signinPromoViewContainerStub The ViewStub that contains the layout element in which
     *     the sign-in promo will be inflated.
     */
    public NtpSigninPromoCoordinator(
            WindowAndroid windowAndroid,
            Activity activity,
            Profile profile,
            ActivityResultTracker activityResultTracker,
            SigninAndHistorySyncActivityLauncher launcher,
            BottomSheetController bottomSheetController,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            SnackbarManager snackbarManager,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            ViewStub signinPromoViewContainerStub,
            BooleanSupplier isSetupListActiveSupplier) {
        mSigninPromoCoordinator =
                new SigninPromoCoordinator(
                        windowAndroid,
                        activity,
                        profile,
                        activityResultTracker,
                        launcher,
                        SupplierUtils.of(bottomSheetController),
                        modalDialogManagerSupplier,
                        snackbarManager,
                        deviceLockActivityLauncher,
                        new NtpSigninPromoDelegate(
                                activity,
                                profile,
                                launcher,
                                this::onPromoStateChange,
                                isSetupListActiveSupplier));

        mSigninPromoViewContainerStub = signinPromoViewContainerStub;
        onPromoStateChange();
    }

    public void destroy() {
        mSigninPromoCoordinator.destroy();
    }

    private void onPromoStateChange() {
        final boolean canShowPromo = mSigninPromoCoordinator.canShowPromo();
        if (canShowPromo && mSigninPromoView == null) {
            inflateView();
        }
        if (mSigninPromoView != null) {
            mSigninPromoView.setVisibility(canShowPromo ? View.VISIBLE : View.GONE);
        }
    }

    private void inflateView() {
        assert mSigninPromoView == null;
        mSigninPromoView = (PersonalizedSigninPromoView) mSigninPromoViewContainerStub.inflate();
        mSigninPromoView.setCardBackgroundResource(R.drawable.home_surface_ui_background);
        mSigninPromoCoordinator.setView(mSigninPromoView);
    }
}
