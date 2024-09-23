// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.MainThread;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.TextViewWithLeading;

/**
 * Creates a dialog that lets users choose whether or not they want to send diagnostic data to
 * Google in the First Run Experience signin screen.
 */
public class UMADialogCoordinator {
    /** Callback for the switch in the dialog. */
    public interface Listener {
        void onAllowMetricsAndCrashUploadingChecked(boolean allowMetricsAndCrashUploading);
    }

    private final ModalDialogManager mDialogManager;
    private final PropertyModel mModel;
    private final View mView;

    /** Constructs the coordinator and shows the dialog. */
    @MainThread
    public UMADialogCoordinator(
            Context context,
            ModalDialogManager modalDialogManager,
            Listener listener,
            boolean allowMetricsAndCrashUploading) {
        mView = LayoutInflater.from(context).inflate(R.layout.uma_dialog, null);
        mDialogManager = modalDialogManager;
        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mView)
                        .with(ModalDialogProperties.CONTROLLER, createController())
                        .build();

        mView.findViewById(R.id.fre_uma_dialog_dismiss_button)
                .setOnClickListener(
                        v -> {
                            mDialogManager.dismissDialog(
                                    mModel, DialogDismissalCause.ACTION_ON_CONTENT);
                        });
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            TextViewWithLeading secondDescriptionView =
                    mView.findViewById(R.id.fre_uma_dialog_second_section_text);
            secondDescriptionView.setText(R.string.signin_fre_uma_dialog_second_section_body);
        }
        final MaterialSwitchWithText umaSwitch = mView.findViewById(R.id.fre_uma_dialog_switch);
        umaSwitch.setChecked(allowMetricsAndCrashUploading);
        umaSwitch.setOnCheckedChangeListener(
                (compoundButton, isChecked) ->
                        listener.onAllowMetricsAndCrashUploadingChecked(isChecked));

        mDialogManager.showDialog(mModel, ModalDialogType.APP);
    }

    void dismissDialogForTesting() {
        mDialogManager.dismissDialog(mModel, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
    }

    View getDialogViewForTesting() {
        return mView;
    }

    private static Controller createController() {
        return new Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {}

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {}
        };
    }
}
