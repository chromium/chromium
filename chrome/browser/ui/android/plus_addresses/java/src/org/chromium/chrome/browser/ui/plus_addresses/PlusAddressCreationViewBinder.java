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
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.PLUS_ADDRESS_ICON_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.PLUS_ADDRESS_LOADING_VIEW_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.PROPOSED_PLUS_ADDRESS;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.REFRESH_ICON_ENABLED;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.REFRESH_ICON_VISIBLE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.SHOW_ONBOARDING_NOTICE;
import static org.chromium.chrome.browser.ui.plus_addresses.PlusAddressCreationProperties.VISIBLE;

import android.graphics.Typeface;
import android.text.style.StyleSpan;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

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
        } else if (propertyKey == PLUS_ADDRESS_ICON_VISIBLE) {
            view.setPlusAddressIconVisible(model.get(PLUS_ADDRESS_ICON_VISIBLE));
        } else if (propertyKey == PLUS_ADDRESS_LOADING_VIEW_VISIBLE) {
            view.setPlusAddressLoadingViewVisible(model.get(PLUS_ADDRESS_LOADING_VIEW_VISIBLE));
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
            bindErrorState(view, model.get(DELEGATE), model.get(ERROR_STATE_INFO));
        } else {
            assert false : "Every possible property update needs to be handled!";
        }
    }

    private static void bindErrorState(
            PlusAddressCreationBottomSheetContent view,
            PlusAddressCreationDelegate delegate,
            @Nullable PlusAddressCreationErrorStateInfo info) {
        if (info == null) {
            view.mPlusAddressContent.setVisibility(View.VISIBLE);
            View errorContainer = view.mContentView.findViewById(R.id.plus_address_error_container);
            // Error screen views are not inflated before the error screen is shown for the
            // first time.
            if (errorContainer != null) {
                errorContainer.setVisibility(View.GONE);
            }
            return;
        }
        // Hide the normal state content.
        view.mPlusAddressContent.setVisibility(View.GONE);

        // Inflate the {@link ViewStub} in case this wasn't done before.
        if (view.mContentView.findViewById(R.id.plus_address_error_container) == null) {
            view.mErrorContentStub.setLayoutResource(R.layout.plus_address_creation_error_state);
            view.mErrorContentStub.inflate();
        }

        // Show the error state container.
        ViewGroup errorStateContainer =
                view.mContentView.findViewById(R.id.plus_address_error_container);
        errorStateContainer.setVisibility(View.VISIBLE);

        // Bind individual views to the data model.
        TextView errorTitle = view.mContentView.findViewById(R.id.plus_address_error_title);
        TextView errorDescription =
                view.mContentView.findViewById(R.id.plus_address_error_description);
        Button okButton = view.mContentView.findViewById(R.id.plus_address_error_ok_button);
        Button cancelButton = view.mContentView.findViewById(R.id.plus_address_error_cancel_button);

        errorTitle.setText(info.getTitle());
        if (info.getErrorType() == PlusAddressCreationBottomSheetErrorType.CREATE_AFFILIATION) {
            errorDescription.setText(
                    SpanApplier.applySpans(
                            info.getDescription(),
                            new SpanInfo("<b1>", "</b1>", new StyleSpan(Typeface.BOLD)),
                            new SpanInfo("<b2>", "</b2>", new StyleSpan(Typeface.BOLD))));
        } else {
            errorDescription.setText(info.getDescription());
        }
        okButton.setText(info.getOkText());
        switch (info.getErrorType()) {
            case PlusAddressCreationBottomSheetErrorType.RESERVE_TIMEOUT:
            case PlusAddressCreationBottomSheetErrorType.RESERVE_GENERIC:
            case PlusAddressCreationBottomSheetErrorType.CREATE_TIMEOUT:
            case PlusAddressCreationBottomSheetErrorType.CREATE_GENERIC:
                okButton.setOnClickListener(unused -> delegate.onTryAgain());
                break;
            case PlusAddressCreationBottomSheetErrorType.RESERVE_QUOTA:
            case PlusAddressCreationBottomSheetErrorType.CREATE_QUOTA:
                okButton.setOnClickListener(unused -> delegate.onCanceled());
                break;
            case PlusAddressCreationBottomSheetErrorType.CREATE_AFFILIATION:
                okButton.setOnClickListener(unused -> delegate.onConfirmRequested());
                break;
            default:
                assert false : "Every possible error type needs to be handled!";
        }
        if (info.getCancelText().isEmpty()) {
            cancelButton.setVisibility(View.GONE);
        } else {
            cancelButton.setText(info.getCancelText());
            cancelButton.setOnClickListener(unused -> delegate.onCanceled());
            cancelButton.setVisibility(View.VISIBLE);
        }

        // Adjust the bottom sheet height after the error screen content is shown.
        view.expandSheet();
    }

    private PlusAddressCreationViewBinder() {}
}
