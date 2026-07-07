// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder class for mapping PropertyModel to EnterpriseSignalsDisclaimerBottomSheetView. */
@NullMarked
class EnterpriseSignalsDisclaimerViewBinder {
    /**
     * Binds property changes in the model to the view.
     *
     * @param model The PropertyModel.
     * @param view The EnterpriseSignalsDisclaimerBottomSheetView.
     * @param propertyKey The key of the property that changed.
     */
    public static void bind(
            PropertyModel model,
            EnterpriseSignalsDisclaimerBottomSheetView view,
            PropertyKey propertyKey) {
        if (propertyKey == EnterpriseSignalsDisclaimerProperties.TITLE) {
            view.setTitle(model.get(EnterpriseSignalsDisclaimerProperties.TITLE));
        } else if (propertyKey == EnterpriseSignalsDisclaimerProperties.DESCRIPTION) {
            view.setDescription(model.get(EnterpriseSignalsDisclaimerProperties.DESCRIPTION));
        } else if (propertyKey == EnterpriseSignalsDisclaimerProperties.PROFILE_INFORMATION_TITLE) {
            view.setProfileInformationTitle(
                    model.get(EnterpriseSignalsDisclaimerProperties.PROFILE_INFORMATION_TITLE));
        } else if (propertyKey
                == EnterpriseSignalsDisclaimerProperties.PROFILE_INFORMATION_DETAILS) {
            view.setProfileInformationDetails(
                    model.get(EnterpriseSignalsDisclaimerProperties.PROFILE_INFORMATION_DETAILS));
        } else if (propertyKey == EnterpriseSignalsDisclaimerProperties.DEVICE_INFORMATION_TITLE) {
            view.setDeviceInformationTitle(
                    model.get(EnterpriseSignalsDisclaimerProperties.DEVICE_INFORMATION_TITLE));
        } else if (propertyKey
                == EnterpriseSignalsDisclaimerProperties.DEVICE_INFORMATION_DETAILS) {
            view.setDeviceInformationDetails(
                    model.get(EnterpriseSignalsDisclaimerProperties.DEVICE_INFORMATION_DETAILS));
        } else if (propertyKey == EnterpriseSignalsDisclaimerProperties.ACCEPT_BUTTON_TEXT) {
            view.setAcceptButtonText(
                    model.get(EnterpriseSignalsDisclaimerProperties.ACCEPT_BUTTON_TEXT));
        } else if (propertyKey == EnterpriseSignalsDisclaimerProperties.CANCEL_BUTTON_TEXT) {
            view.setCancelButtonText(
                    model.get(EnterpriseSignalsDisclaimerProperties.CANCEL_BUTTON_TEXT));
        } else if (propertyKey == EnterpriseSignalsDisclaimerProperties.ON_ACCEPT_CLICKED) {
            view.setOnAcceptClicked(
                    model.get(EnterpriseSignalsDisclaimerProperties.ON_ACCEPT_CLICKED));
        } else if (propertyKey == EnterpriseSignalsDisclaimerProperties.ON_CANCEL_CLICKED) {
            view.setOnCancelClicked(
                    model.get(EnterpriseSignalsDisclaimerProperties.ON_CANCEL_CLICKED));
        }
    }

    private EnterpriseSignalsDisclaimerViewBinder() {}
}
