// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.MainThread;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerCoordinator.Listener;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;

/** Coordinator to show the modal account picker dialog. */
public class AccountPickerDialogCoordinator {
    private final RecyclerView mAccountPickerView;
    private final AccountPickerCoordinator mCoordinator;
    private final ModalDialogManager mDialogManager;
    private final PropertyModel mModel;

    /** Constructs the coordinator and shows the account picker dialog. */
    @MainThread
    public AccountPickerDialogCoordinator(
            Context context, Listener listener, ModalDialogManager modalDialogManager) {
        mDialogManager = modalDialogManager;
        mAccountPickerView = inflateAccountPickerView(context);

        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            mCoordinator =
                    new AccountPickerCoordinator(
                            mAccountPickerView,
                            listener,
                            R.layout.account_picker_dialog_row,
                            R.layout.account_picker_dialog_new_account_row);
        } else {
            mCoordinator =
                    new AccountPickerCoordinator(
                            mAccountPickerView,
                            listener,
                            R.layout.account_picker_row,
                            R.layout.account_picker_new_account_row);
        }
        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.TITLE,
                                context.getString(R.string.signin_account_picker_dialog_title))
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mAccountPickerView)
                        .with(ModalDialogProperties.CONTROLLER, createController())
                        .build();

        mDialogManager.showDialog(mModel, ModalDialogType.APP);
    }

    /** Dismisses the account picker dialog. */
    @MainThread
    public void dismissDialog() {
        mDialogManager.dismissDialog(mModel, DialogDismissalCause.ACTION_ON_CONTENT);
    }

    private static RecyclerView inflateAccountPickerView(Context context) {
        LayoutInflater inflater = LayoutInflater.from(context);
        RecyclerView accountPickerView =
                (RecyclerView) inflater.inflate(R.layout.account_picker_dialog_body, null);
        accountPickerView.setLayoutManager(new LinearLayoutManager(context));
        return accountPickerView;
    }

    private Controller createController() {
        return new Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {}

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {
                mCoordinator.destroy();
            }
        };
    }

    View getAccountPickerViewForTests() {
        return mAccountPickerView;
    }
}
