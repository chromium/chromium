// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator for the enterprise signals disclaimer.
 *
 * <p>This disclaimer is shown on browser startup to managed users missing the consent for
 * collecting the device signals. The dialog offers two choices: Accept or Decline. Accepting the
 * dialog dismisses it and allows the user to proceed with using the browser while also marking the
 * consent as granted. Dismissing the dialog with a gesture or explicitly clicking 'Sign out' will
 * close the profile and open the profile picker.
 */
@NullMarked
class EnterpriseSignalsDisclaimerMediator {
    private final PropertyModel mModel;

    EnterpriseSignalsDisclaimerMediator(Context context) {
        // TODO(b/512836948): Replace with localized strings once the content is finalized.
        mModel =
                new PropertyModel.Builder(EnterpriseSignalsDisclaimerProperties.ALL_KEYS)
                        .with(
                                EnterpriseSignalsDisclaimerProperties.TITLE,
                                "Your work secured on Chrome")
                        .with(
                                EnterpriseSignalsDisclaimerProperties.DESCRIPTION,
                                "To secure your work on Chrome, your organization will be able to"
                                    + " view or manage certain information when you're signed-in to"
                                    + " Chrome")
                        .with(
                                EnterpriseSignalsDisclaimerProperties.PROFILE_INFORMATION_TITLE,
                                "Profile information")
                        .with(
                                EnterpriseSignalsDisclaimerProperties.PROFILE_INFORMATION_DETAILS,
                                "Your organization may need to see and manage browsing data in your"
                                    + " work profile, such as your browsing history and passwords")
                        .with(
                                EnterpriseSignalsDisclaimerProperties.DEVICE_INFORMATION_TITLE,
                                "Device information")
                        .with(
                                EnterpriseSignalsDisclaimerProperties.DEVICE_INFORMATION_DETAILS,
                                "To make sure this device can be used safely, your organization may"
                                    + " need to see information about its operating system,"
                                    + " browser, settings, and what software is installed on the"
                                    + " device")
                        .with(EnterpriseSignalsDisclaimerProperties.ACCEPT_BUTTON_TEXT, "Got it")
                        .with(EnterpriseSignalsDisclaimerProperties.CANCEL_BUTTON_TEXT, "Sign out")
                        .with(
                                EnterpriseSignalsDisclaimerProperties.ON_ACCEPT_CLICKED,
                                v -> onAccept())
                        .with(
                                EnterpriseSignalsDisclaimerProperties.ON_CANCEL_CLICKED,
                                v -> onCancel())
                        .build();
    }

    /**
     * Returns the PropertyModel managed by this mediator.
     *
     * @return The PropertyModel.
     */
    public PropertyModel getModel() {
        return mModel;
    }

    /** Dismisses the dialog and marks the device signals collection consent as granted. */
    void onAccept() {}

    /**
     * Closes the profile and opens the profile picker. This is also triggered when the dialog is
     * dismissed by a back press, sliding down or tapping outside of the dialog area.
     */
    void onCancel() {}
}
