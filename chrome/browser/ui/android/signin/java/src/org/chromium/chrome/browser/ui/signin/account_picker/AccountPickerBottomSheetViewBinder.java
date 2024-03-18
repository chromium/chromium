// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetProperties.ViewState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Stateless AccountPickerBottomSheet view binder. */
class AccountPickerBottomSheetViewBinder {
    static void bind(
            PropertyModel model, AccountPickerBottomSheetView view, PropertyKey propertyKey) {
        if (propertyKey == AccountPickerBottomSheetProperties.ON_SELECTED_ACCOUNT_CLICKED) {
            view.getSelectedAccountView()
                    .setOnClickListener(
                            model.get(
                                    AccountPickerBottomSheetProperties
                                            .ON_SELECTED_ACCOUNT_CLICKED));
        } else if (propertyKey == AccountPickerBottomSheetProperties.VIEW_STATE) {
            @ViewState int viewState = model.get(AccountPickerBottomSheetProperties.VIEW_STATE);
            view.setDisplayedView(viewState);
        } else if (propertyKey == AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA) {
            DisplayableProfileData profileData =
                    model.get(AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA);
            if (profileData != null) {
                view.updateSelectedAccount(profileData);
            }
        } else if (propertyKey == AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DOMAIN) {
            view.updateSelectedDomain(
                    model.get(AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DOMAIN));
        } else if (propertyKey == AccountPickerBottomSheetProperties.ON_CONTINUE_AS_CLICKED) {
            view.setOnClickListenerOfContinueButton(
                    model.get(AccountPickerBottomSheetProperties.ON_CONTINUE_AS_CLICKED));
        } else if (propertyKey == AccountPickerBottomSheetProperties.ON_DISMISS_CLICKED) {
            view.getDismissButton()
                    .setOnClickListener(
                            model.get(AccountPickerBottomSheetProperties.ON_DISMISS_CLICKED));
        } else if (propertyKey == AccountPickerBottomSheetProperties.BOTTOM_SHEET_STRINGS) {
            AccountPickerBottomSheetStrings bottomSheetStrings =
                    model.get(AccountPickerBottomSheetProperties.BOTTOM_SHEET_STRINGS);
            view.setBottomSheetStrings(
                    bottomSheetStrings.titleStringId,
                    bottomSheetStrings.subtitleStringId,
                    bottomSheetStrings.dismissButtonStringId);
        }
    }

    private AccountPickerBottomSheetViewBinder() {}
}
