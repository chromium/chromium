// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fre;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Switch;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Creates a dialog that lets users choose whether or not they want to send diagnostic data to
 * Google in the First Run Experience signin screen.
 */
public class FreUMADialogCoordinator {
    /**
     * Callback for the switch in the dialog.
     */
    public interface Listener {
        void onAllowMetricsAndCrashUploadingChecked(boolean allowMetricsAndCrashUploading);
    }

    private final ModalDialogManager mDialogManager;
    private final PropertyModel mModel;
    private final View mView;

    /**
     * Constructs the coordinator and shows the dialog.
     */
    @MainThread
    public FreUMADialogCoordinator(Context context, ModalDialogManager modalDialogManager,
            Listener listener, boolean allowMetricsAndCrashUploading) {
        mView = LayoutInflater.from(context).inflate(R.layout.fre_uma_dialog, null);
        mDialogManager = modalDialogManager;
        mModel = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                         .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                         .with(ModalDialogProperties.CUSTOM_VIEW, mView)
                         .with(ModalDialogProperties.CONTROLLER, createController())
                         .build();

        mView.findViewById(R.id.fre_uma_dialog_dismiss_button).setOnClickListener(v -> {
            mDialogManager.dismissDialog(mModel, DialogDismissalCause.ACTION_ON_CONTENT);
        });
        final Switch umaSwitch = mView.findViewById(R.id.fre_uma_dialog_switch);
        umaSwitch.setChecked(allowMetricsAndCrashUploading);
        umaSwitch.setOnCheckedChangeListener(
                (compoundButton,
                        isChecked) -> listener.onAllowMetricsAndCrashUploadingChecked(isChecked));

        mDialogManager.showDialog(mModel, ModalDialogType.APP);
    }

    @VisibleForTesting
    void dismissDialogForTesting() {
        mDialogManager.dismissDialog(mModel, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
    }

    @VisibleForTesting
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
