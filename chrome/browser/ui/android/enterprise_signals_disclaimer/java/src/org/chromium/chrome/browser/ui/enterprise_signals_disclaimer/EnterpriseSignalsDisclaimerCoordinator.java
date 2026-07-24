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
 * startup and on primary account change for managed enterprise users who have not acknoledged the
 * disclaimer previosuly.
 */
@NullMarked
public class EnterpriseSignalsDisclaimerCoordinator {
    private final BottomSheetController mBottomSheetController;
    private final EnterpriseSignalsDisclaimerBottomSheetView mSheetContent;
    private final EnterpriseSignalsDisclaimerMediator mMediator;
    private final PropertyModelChangeProcessor mModelChangeProcessor;

    /**
     * Constructs an {@link EnterpriseSignalsDisclaimerCoordinator}.
     *
     * <p>This class should only be instantiated if the primary account is set and managed.
     *
     * @param context The Android {@link Context}.
     * @param bottomSheetController The {@link BottomSheetController} for showing the bottom sheet.
     * @param signinManager The {@link SigninManager} for checking management status and fetching
     *     the profile picture.
     */
    public EnterpriseSignalsDisclaimerCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            SigninManager signinManager) {
        mBottomSheetController = bottomSheetController;
        mSheetContent = new EnterpriseSignalsDisclaimerBottomSheetView(context);

        final IdentityManager identityManager = signinManager.getIdentityManager();
        assert identityManager.hasPrimaryAccount();

        mMediator = new EnterpriseSignalsDisclaimerMediator(context, identityManager);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(),
                        mSheetContent,
                        EnterpriseSignalsDisclaimerViewBinder::bind);
    }

    /** Shows the enterprise signals disclaimer bottom sheet. */
    public void show() {
        mBottomSheetController.requestShowContent(mSheetContent, /* animate= */ true);
    }

    /** Destroys the coordinator, hiding the sheet and cleaning up resources. */
    public void destroy() {
        mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
        mModelChangeProcessor.destroy();
        mMediator.destroy();
    }
}
