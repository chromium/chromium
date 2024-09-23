// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import android.view.View.OnClickListener;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Properties of account picker bottom sheet. */
class AccountPickerBottomSheetProperties {
    /**
     * View states of account picker. Different account picker view state correspond to different
     * account picker bottom sheet configuration.
     */
    @IntDef({
        ViewState.NO_ACCOUNTS,
        ViewState.COLLAPSED_ACCOUNT_LIST,
        ViewState.EXPANDED_ACCOUNT_LIST,
        ViewState.SIGNIN_IN_PROGRESS,
        ViewState.SIGNIN_GENERAL_ERROR,
        ViewState.SIGNIN_AUTH_ERROR,
        ViewState.CONFIRM_MANAGEMENT,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ViewState {
        /**
         * When there is no account on device, the user sees only one blue button
         * |Add account to device|.
         *
         * The bottom sheet starts with this state when there is zero account on device.
         */
        int NO_ACCOUNTS = 0;

        /**
         * When the account list is collapsed with exactly one account shown as the selected
         * account and a blue |Continue as| button to sign in with the selected account.
         *
         * The bottom sheet starts with this state when there is at least one account on device.
         *
         * This state can also be reached from EXPANDED_ACCOUNT_LIST by clicking on one of
         * the accounts in the expanded account list.
         */
        int COLLAPSED_ACCOUNT_LIST = 1;

        /**
         * When the account list is expanded, the user sees the account list of all the accounts
         * on device and an additional row like |Add account to device|.
         *
         * This state is reached from COLLAPSED_ACCOUNT_LIST by clicking the selected account of
         * the collapsed account list.
         */
        int EXPANDED_ACCOUNT_LIST = 2;

        /**
         * When the user is in the sign-in process, no account or button will be visible, the user
         * sees mainly a spinner in the bottom sheet.
         *
         * <p>This state is initially reached from COLLAPSED_ACCOUNT_LIST, when the button |Continue
         * as| is clicked. This state may lead to/from CONFIRM_MANAGEMENT, if required.
         */
        int SIGNIN_IN_PROGRESS = 3;

        /**
         * When user cannot complete sign-in due to connectivity issues for example, the
         * general sign-in error screen will be shown.
         *
         * The state can be reached when an error appears during the sign-in process.
         */
        int SIGNIN_GENERAL_ERROR = 4;

        /**
         * When user cannot complete sign-in due to invalidate credential, the
         * sign-in auth error screen will be shown.
         *
         * The state can be reached when an auth error appears during the sign-in process.
         */
        int SIGNIN_AUTH_ERROR = 5;

        /**
         * When the signin is in progress for a managed account that requires to user to confirm
         * account management.
         *
         * <p>This state may be reached from SIGNIN_IN_PROGRESS, which will be returned to after the
         * user clicks "Accept and Sign in".
         */
        int CONFIRM_MANAGEMENT = 6;
    }

    // PropertyKeys for the selected account view when the account list is collapsed.
    // The selected account view is replaced by account list view when the
    // account list is expanded.
    static final ReadableObjectPropertyKey<OnClickListener> ON_SELECTED_ACCOUNT_CLICKED =
            new ReadableObjectPropertyKey<>("on_selected_account_clicked");
    static final WritableObjectPropertyKey<DisplayableProfileData> SELECTED_ACCOUNT_DATA =
            new WritableObjectPropertyKey<>("selected_account_data");

    // PropertyKey for the Account Management confirmation domain.
    static final WritableObjectPropertyKey<String> SELECTED_ACCOUNT_DOMAIN =
            new WritableObjectPropertyKey<>("selected_account_domain");

    // PropertyKey for the button |Continue as ...|
    static final ReadableObjectPropertyKey<OnClickListener> ON_CONTINUE_AS_CLICKED =
            new ReadableObjectPropertyKey<>("on_continue_as_clicked");

    // PropertyKey for the button to dismiss the bottom sheet
    static final ReadableObjectPropertyKey<OnClickListener> ON_DISMISS_CLICKED =
            new ReadableObjectPropertyKey<>("on_dismiss_clicked");

    // PropertyKey indicates the view state of the account picker bottom sheet
    static final WritableIntPropertyKey VIEW_STATE = new WritableIntPropertyKey("view_state");

    // PropertyKey indicating the title, subtitle, and cancel text for the bottom sheet.
    static final ReadableObjectPropertyKey<AccountPickerBottomSheetStrings> BOTTOM_SHEET_STRINGS =
            new ReadableObjectPropertyKey("bottom_sheet_strings");

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ON_SELECTED_ACCOUNT_CLICKED,
                SELECTED_ACCOUNT_DATA,
                SELECTED_ACCOUNT_DOMAIN,
                ON_CONTINUE_AS_CLICKED,
                ON_DISMISS_CLICKED,
                VIEW_STATE,
                BOTTOM_SHEET_STRINGS,
            };

    /**
     * Creates a default model for the AccountPickerBottomSheet.
     *
     * In the default model, as the selected account data is null, the bottom sheet is in the
     * state {@link ViewState#NO_ACCOUNTS}.
     */
    static PropertyModel createModel(
            Runnable onSelectedAccountClicked,
            Runnable onContinueAsClicked,
            OnClickListener onDismissClicked,
            AccountPickerBottomSheetStrings accountPickerBottomSheetStrings) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(ON_SELECTED_ACCOUNT_CLICKED, v -> onSelectedAccountClicked.run())
                .with(SELECTED_ACCOUNT_DATA, null)
                .with(ON_CONTINUE_AS_CLICKED, v -> onContinueAsClicked.run())
                .with(ON_DISMISS_CLICKED, onDismissClicked)
                .with(VIEW_STATE, ViewState.NO_ACCOUNTS)
                .with(BOTTOM_SHEET_STRINGS, accountPickerBottomSheetStrings)
                .build();
    }

    private AccountPickerBottomSheetProperties() {}
}
