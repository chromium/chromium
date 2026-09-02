// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the enterprise signals disclaimer bottom sheet. The disclaimer is shown on
 * startup and on primary account change for managed enterprise users who have not acknowledged the
 * disclaimer previously.
 */
@NullMarked
public class EnterpriseSignalsDisclaimerCoordinator {
    /** Delegate for the enterprise signals disclaimer. */
    public interface Delegate {
        /**
         * Opens the info page for the given URL.
         *
         * @param url The URL of the webpage to show.
         */
        void showInfoPage(String url);
    }

    private final EnterpriseSignalsDisclaimerMediator mMediator;
    private final PropertyModelChangeProcessor mModelChangeProcessor;
    private final EnterpriseSignalsDisclaimerHost mDisclaimerHost;
    private boolean mIsDestroyed;
    private @Nullable Runnable mOnDestroyCallback;

    /**
     * Constructs an {@link EnterpriseSignalsDisclaimerCoordinator}.
     *
     * <p>This class should only be instantiated if the primary account is set and managed.
     *
     * @param context The Android {@link Context}.
     * @param bottomSheetController The {@link BottomSheetController} for showing the bottom sheet.
     * @param modalDialogManager The {@link ModalDialogManager} for showing the modal dialog.
     * @param signinManager The {@link SigninManager} for checking management status and fetching
     *     the profile picture.
     * @param delegate The {@link Delegate} for embedder interactions.
     */
    public EnterpriseSignalsDisclaimerCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            SigninManager signinManager,
            Delegate delegate,
            Runnable onDestroyCallback) {
        mIsDestroyed = false;
        mOnDestroyCallback = onDestroyCallback;
        final IdentityManager identityManager = signinManager.getIdentityManager();
        assert identityManager.hasPrimaryAccount();

        EnterpriseSignalsDisclaimerView view;
        // For the large form factors a modal dialog will be displayed, while smaller screens will
        // get a bottom sheet.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)) {
            view = EnterpriseSignalsDisclaimerView.createForModalDialog(context);
            mDisclaimerHost =
                    new ModalDialogDisclaimerHost(
                            modalDialogManager, view, this::onDialogDismissed);
        } else {
            var sheetContent = new EnterpriseSignalsDisclaimerBottomSheetView(context);
            view = sheetContent;
            mDisclaimerHost =
                    new BottomSheetDisclaimerHost(
                            bottomSheetController, sheetContent, this::onDialogDismissed);
        }

        mMediator =
                new EnterpriseSignalsDisclaimerMediator(
                        context, identityManager, delegate, signinManager, mDisclaimerHost::hide);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(), view, EnterpriseSignalsDisclaimerViewBinder::bind);
    }

    /**
     * Attempts to show the enterprise signals disclaimer. If the dialog cannot be shown it will be
     * put in a queue and shown whenever possible.
     */
    public void show() {
        assert !mIsDestroyed;
        mDisclaimerHost.show();
    }

    /**
     * @return true if dialog is being shown or is in queue, false otherwise.
     */
    public boolean isActive() {
        return !mIsDestroyed && mDisclaimerHost.isActive();
    }

    /** Destroys the coordinator, hiding the sheet and cleaning up resources. */
    public void destroy() {
        if (mIsDestroyed) {
            return;
        }
        mIsDestroyed = true;
        mDisclaimerHost.destroy();
        mModelChangeProcessor.destroy();
        mMediator.destroy();
        if (mOnDestroyCallback != null) {
            mOnDestroyCallback.run();
            mOnDestroyCallback = null;
        }
    }

    private void onDialogDismissed(boolean reasonWasUserAction) {
        if (mIsDestroyed) {
            return;
        }
        if (reasonWasUserAction) {
            // The user should not be signed out if the dialog is being dismissed by an external
            // force - for instance, the Controller being destroyed.
            mMediator.signOutUser();
        }
        destroy();
    }
}
