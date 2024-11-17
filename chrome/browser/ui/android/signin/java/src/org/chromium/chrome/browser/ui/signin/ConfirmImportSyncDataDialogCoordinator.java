// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.MainThread;
import androidx.annotation.StringRes;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.signin.AccountEmailDisplayHook;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.List;
import java.util.function.Predicate;

/**
 * A coordinator to show a modal dialog that is displayed when the user switches account they are
 * syncing to. It gives the option to merge the data of the two accounts or to keep them separate.
 */
public class ConfirmImportSyncDataDialogCoordinator {
    /**
     * Callback for completion of the dialog. Only one of {@link Listener#onConfirm} or
     * {@link Listener#onCancel} will be called and it will only be called once.
     */
    interface Listener {
        /**
         * The user has completed the dialog using the positive button.
         * @param wipeData Whether the user requested that existing data should be wiped.
         */
        void onConfirm(boolean wipeData);

        /** The user dismisses the dialog through any means other than the positive button. */
        void onCancel();
    }

    private final Listener mListener;

    private final View mConfirmImportSyncDataView;
    private final RadioButtonWithDescription mConfirmImportOption;
    private final RadioButtonWithDescription mKeepSeparateOption;

    private final PropertyModel mModel;
    private final ModalDialogManager mDialogManager;
    private final Predicate<String> mCheckIfDisplayableEmailAddress;

    public ConfirmImportSyncDataDialogCoordinator(
            Context context,
            ModalDialogManager dialogManager,
            Listener listener,
            String currentAccountName,
            String newAccountName,
            boolean isCurrentAccountManaged,
            boolean usesSplitStoresAndUPMForLocal) {
        this(
                context,
                dialogManager,
                listener,
                currentAccountName,
                newAccountName,
                AccountEmailDisplayHook::canHaveEmailAddressDisplayed,
                isCurrentAccountManaged,
                usesSplitStoresAndUPMForLocal);
    }

    /**
     * Creates a new instance of ConfirmImportSyncDataDialogCoordinator and shows a dialog that
     * gives the user the option to merge data between the account they are attempting to sign in to
     * and the account they are currently signed into, or to keep the data separate. This dialog is
     * shown before signing out the current sync account.
     *
     * @param context Context to create the view.
     * @param listener Callback to be called if the user completes the dialog (as opposed to hitting
     *     cancel).
     * @param currentAccountName The current sync account name.
     * @param newAccountName The potential next sync account name.
     * @param checkIfDisplayableEmailAddress Predicate testing if an email is displayable.
     * @param isCurrentAccountManaged Whether the current account is a managed account.
     * @param usesSplitStoresAndUPMForLocal See password_manager::UsesSplitStoresAndUPMForLocal().
     */
    @MainThread
    public ConfirmImportSyncDataDialogCoordinator(
            Context context,
            ModalDialogManager dialogManager,
            Listener listener,
            String currentAccountName,
            String newAccountName,
            Predicate<String> checkIfDisplayableEmailAddress,
            boolean isCurrentAccountManaged,
            boolean usesSplitStoresAndUPMForLocal) {
        mCheckIfDisplayableEmailAddress = checkIfDisplayableEmailAddress;

        mListener = listener;
        mConfirmImportSyncDataView =
                LayoutInflater.from(context).inflate(R.layout.confirm_import_sync_data, null);
        mConfirmImportOption =
                mConfirmImportSyncDataView.findViewById(R.id.sync_confirm_import_choice);
        mKeepSeparateOption =
                mConfirmImportSyncDataView.findViewById(R.id.sync_keep_separate_choice);

        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                context.getString(R.string.continue_button))
                        // For non-managed accounts, the confirmation button starts out disabled,
                        // since none of the options are chosen by default.
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_DISABLED,
                                !isCurrentAccountManaged)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                context.getString(R.string.cancel))
                        .with(ModalDialogProperties.CUSTOM_VIEW, mConfirmImportSyncDataView)
                        .with(ModalDialogProperties.CONTROLLER, createController())
                        .build();
        mDialogManager = dialogManager;

        setUpConfirmImportSyncDataView(
                context,
                currentAccountName,
                newAccountName,
                isCurrentAccountManaged,
                usesSplitStoresAndUPMForLocal);
        mDialogManager.showDialog(mModel, ModalDialogType.APP);
    }

    /** Dismisses the confirm import sync data dialog. */
    @MainThread
    public void dismissDialog() {
        mDialogManager.dismissDialog(mModel, DialogDismissalCause.UNKNOWN);
    }

    private Controller createController() {
        return new Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                if (buttonType == ButtonType.POSITIVE) {
                    assert mConfirmImportOption.isChecked() ^ mKeepSeparateOption.isChecked();

                    RecordUserAction.record(
                            mKeepSeparateOption.isChecked()
                                    ? "Signin_ImportDataPrompt_DontImport"
                                    : "Signin_ImportDataPrompt_ImportData");
                    mListener.onConfirm(mKeepSeparateOption.isChecked());
                    mDialogManager.dismissDialog(
                            mModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                } else if (buttonType == ButtonType.NEGATIVE) {
                    mDialogManager.dismissDialog(
                            mModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                }
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {
                if (dismissalCause == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE
                        || dismissalCause == DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
                    RecordUserAction.record("Signin_ImportDataPrompt_Cancel");
                    mListener.onCancel();
                }
            }
        };
    }

    private void setUpConfirmImportSyncDataView(
            Context context,
            String currentAccountName,
            String newAccountName,
            boolean isCurrentAccountManaged,
            boolean usesSplitStoresAndUPMForLocal) {
        TextView prompt = mConfirmImportSyncDataView.findViewById(R.id.sync_import_data_prompt);

        // The "Combine my data" option only applies to passwords if `usesSplitStoresAndUPMForLocal`
        // is false. Otherwise, don't mention "passwords" in the text. Similarly, "Keep my data
        // separate" only needs to delete local passwords if `usesSplitStoresAndUPMForLocal` is
        // false.
        // TODO(crbug.com/325620996): Plumb the fact that passwords should not be deleted to both
        // the Java and C++ backends, rather than special casing all around.
        @StringRes
        int promptText =
                usesSplitStoresAndUPMForLocal
                        ? R.string.sync_import_data_prompt_without_passwords
                        : R.string.sync_import_data_prompt;
        String displayedAccount =
                mCheckIfDisplayableEmailAddress.test(currentAccountName)
                        ? currentAccountName
                        : context.getString(R.string.default_google_account_username);
        prompt.setText(context.getString(promptText, displayedAccount));

        mConfirmImportOption.setDescriptionText(
                context.getString(R.string.sync_import_existing_data_subtext, newAccountName));
        mKeepSeparateOption.setDescriptionText(
                context.getString(R.string.sync_keep_existing_data_separate_subtext_existing_data));

        List<RadioButtonWithDescription> radioGroup =
                Arrays.asList(mConfirmImportOption, mKeepSeparateOption);
        mConfirmImportOption.setRadioButtonGroup(radioGroup);
        mKeepSeparateOption.setRadioButtonGroup(radioGroup);

        // If the account is managed, disallow merging information.
        if (isCurrentAccountManaged) {
            mKeepSeparateOption.setChecked(true);
            mConfirmImportOption.setOnClickListener(
                    view -> ManagedPreferencesUtils.showManagedByAdministratorToast(context));
        } else {
            // The confirmation button gets enabled as soon as either of the radio button options
            // was selected.
            mKeepSeparateOption.setOnCheckedChangeListener(
                    radioButton ->
                            mModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, false));
            mConfirmImportOption.setOnCheckedChangeListener(
                    radioButton ->
                            mModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, false));
        }
    }
}
