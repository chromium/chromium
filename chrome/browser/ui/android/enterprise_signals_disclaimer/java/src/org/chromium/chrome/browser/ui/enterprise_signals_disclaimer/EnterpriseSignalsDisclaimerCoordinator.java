// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.identitymanager.IdentityManager;
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

    /**
     * Constructs an {@link EnterpriseSignalsDisclaimerCoordinator}.
     *
     * <p>This class should only be instantiated if the primary account is set and managed.
     *
     * @param context The Android {@link Context}.
     * @param bottomSheetController The {@link BottomSheetController} for showing the bottom sheet.
     * @param signinManager The {@link SigninManager} for checking management status and fetching
     *     the profile picture.
     * @param delegate The {@link Delegate} for embedder interactions.
     */
    public EnterpriseSignalsDisclaimerCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            SigninManager signinManager,
            Delegate delegate) {
        final IdentityManager identityManager = signinManager.getIdentityManager();
        assert identityManager.hasPrimaryAccount();
        mMediator = new EnterpriseSignalsDisclaimerMediator(context, identityManager, delegate);

        var view = new EnterpriseSignalsDisclaimerBottomSheetView(context);
        mDisclaimerHost = new BottomSheetDisclaimerHost(bottomSheetController, view);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(), view, EnterpriseSignalsDisclaimerViewBinder::bind);
    }

    /**
     * Attempts to show the enterprise signals disclaimer. If the dialog cannot be shown it will be
     * put in a queue and shown whenever possible.
     */
    public void show() {
        mDisclaimerHost.show();
    }

    /**
     * @return true if dialog is being shown or is in queue, false otherwise.
     */
    public boolean isActive() {
        return mDisclaimerHost.isActive();
    }

    /** Destroys the coordinator, hiding the sheet and cleaning up resources. */
    public void destroy() {
        mDisclaimerHost.hide();
        mModelChangeProcessor.destroy();
        mMediator.destroy();
    }
}
