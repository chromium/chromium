// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.CANCEL_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.CONFIRM_BUTTON_ENABLED;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.CONFIRM_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.DELEGATE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.ERROR_STATE_INFO;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.LEGACY_ERROR_REPORTING_INSTRUCTION_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.LOADING_INDICATOR_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.NORMAL_STATE_INFO;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.PROPOSED_PLUS_ADDRESS;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.REFRESH_ICON_ENABLED;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.REFRESH_ICON_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.SHOW_ONBOARDING_NOTICE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.VISIBLE;

import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Binds the {@code PlusAddressCreationProperties} to the {@PlusAddressCreationBottomSheetContent}.
 */
class PlusAddressCreationViewBinder {

    static void bindPlusAddressCreationBottomSheet(
            PropertyModel model,
            PlusAddressCreationBottomSheetContent view,
            PropertyKey propertyKey) {
        if (propertyKey == NORMAL_STATE_INFO) {
            PlusAddressCreationNormalStateInfo info = model.get(NORMAL_STATE_INFO);
            view.mTitleView.setText(info.getTitle());
            view.mDescriptionView.setText(info.getDescription());
            if (model.get(SHOW_ONBOARDING_NOTICE)) {
                view.setOnboardingNotice(info.getNotice(), info.getLearnMoreUrl());
            } else {
                view.mFirstTimeNotice.setVisibility(View.GONE);
            }
            view.setLegacyErrorReportingInstruction(
                    info.getErrorReportInstruction(), info.getErrorReportUrl());
            view.mPlusAddressConfirmButton.setText(info.getConfirmText());
            view.mPlusAddressCancelButton.setText(info.getCancelText());
        } else if (propertyKey == DELEGATE) {
            view.setDelegate(model.get(DELEGATE));
        } else if (propertyKey == SHOW_ONBOARDING_NOTICE) {
            // This property doesn't require any binding logic.
        } else if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else if (propertyKey == PROPOSED_PLUS_ADDRESS) {
            view.setProposedPlusAddress(model.get(PROPOSED_PLUS_ADDRESS));
        } else if (propertyKey == REFRESH_ICON_ENABLED) {
            view.setRefreshIconEnabled(model.get(REFRESH_ICON_ENABLED));
        } else if (propertyKey == REFRESH_ICON_VISIBLE) {
            view.setRefreshIconVisible(model.get(REFRESH_ICON_VISIBLE));
        } else if (propertyKey == CONFIRM_BUTTON_ENABLED) {
            view.setConfirmButtonEnabled(model.get(CONFIRM_BUTTON_ENABLED));
        } else if (propertyKey == CONFIRM_BUTTON_VISIBLE) {
            view.setConfirmButtonVisible(model.get(CONFIRM_BUTTON_VISIBLE));
        } else if (propertyKey == CANCEL_BUTTON_VISIBLE) {
            view.setCancelButtonVisible(model.get(CANCEL_BUTTON_VISIBLE));
        } else if (propertyKey == LEGACY_ERROR_REPORTING_INSTRUCTION_VISIBLE) {
            view.setLegacyErrorReportingInstructionVisible(
                    model.get(LEGACY_ERROR_REPORTING_INSTRUCTION_VISIBLE));
        } else if (propertyKey == LOADING_INDICATOR_VISIBLE) {
            view.setLoadingIndicatorVisible(model.get(LOADING_INDICATOR_VISIBLE));
        } else if (propertyKey == ERROR_STATE_INFO) {
            // Hide the normal state content.
            view.mPlusAddressContent.setVisibility(View.GONE);
            // Show the error state content.
            view.mErrorContentStub.setLayoutResource(R.layout.plus_address_creation_error_state);
            view.mErrorContentStub.inflate();

            TextView errorTitle = view.mContentView.findViewById(R.id.plus_address_error_title);
            TextView errorDescription =
                    view.mContentView.findViewById(R.id.plus_address_error_description);
            Button okButton = view.mContentView.findViewById(R.id.plus_address_error_ok_button);
            Button cancelButton =
                    view.mContentView.findViewById(R.id.plus_address_error_cancel_button);

            PlusAddressCreationErrorStateInfo info = model.get(ERROR_STATE_INFO);
            errorTitle.setText(info.getTitle());
            errorDescription.setText(info.getDescription());
            okButton.setText(info.getOkText());
            cancelButton.setText(info.getCancelText());
        } else {
            assert false : "Every possible property update needs to be handled!";
        }
    }

    private PlusAddressCreationViewBinder() {}
}
