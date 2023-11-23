// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions.nfc;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Build;
import android.provider.Settings;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.core.widget.TextViewCompat;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.permissions.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Implements a modal dialog that prompts the user about turning on the NFC adapter on the system
 * level.
 */
public class NfcSystemLevelPrompt implements ModalDialogProperties.Controller {
    private ModalDialogManager mModalDialogManager;
    private WindowAndroid mWindowAndroid;
    private Runnable mCallback;

    /**
     * Triggers a prompt to ask the user to turn on the system NFC setting on their device.
     *
     * <p>The prompt will be triggered within the specified window.
     *
     * @param window The current window to display the prompt into.
     * @param callback The callback to be called when dialog is closed.
     */
    public void show(WindowAndroid window, Runnable callback) {
        ModalDialogManager modalDialogManager = window.getModalDialogManager();
        if (modalDialogManager == null) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, () -> callback.run());
            return;
        }
        show(window, modalDialogManager, callback);
    }

    @VisibleForTesting
    protected void show(
            WindowAndroid window, ModalDialogManager modalDialogManager, Runnable callback) {
        Activity activity = window.getActivity().get();
        LayoutInflater inflater = LayoutInflater.from(activity);
        View customView = inflater.inflate(R.layout.permission_dialog, null);

        TextView messageTextView = customView.findViewById(R.id.text);
        messageTextView.setText(R.string.nfc_disabled_on_device_message);
        TextViewCompat.setCompoundDrawablesRelativeWithIntrinsicBounds(
                messageTextView, R.drawable.gm_filled_nfc_24, 0, 0, 0);

        Resources resources = activity.getResources();
        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                R.string.nfc_prompt_turn_on)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.cancel)
                        .with(
                                ModalDialogProperties.CONTENT_DESCRIPTION,
                                resources,
                                R.string.nfc_disabled_on_device_message)
                        .with(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY, true)
                        .build();

        mWindowAndroid = window;
        mCallback = callback;
        mModalDialogManager = modalDialogManager;
        mModalDialogManager.showDialog(model, ModalDialogManager.ModalDialogType.TAB);
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        } else {
            String nfcAction =
                    (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q)
                            ? Settings.Panel.ACTION_NFC
                            : Settings.ACTION_NFC_SETTINGS;
            Intent intent = new Intent(nfcAction);
            try {
                mWindowAndroid.showIntent(
                        intent,
                        new WindowAndroid.IntentCallback() {
                            @Override
                            public void onIntentCompleted(int resultCode, Intent data) {
                                mModalDialogManager.dismissDialog(
                                        model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                            }
                        },
                        null);
            } catch (android.content.ActivityNotFoundException ex) {
                mModalDialogManager.dismissDialog(
                        model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
            }
        }
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        if (mCallback != null) mCallback.run();
        mCallback = null;
    }
}
