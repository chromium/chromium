// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission;

import android.widget.TextView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;
import org.chromium.ui.widget.ButtonCompat;

/**
 * This class is responsible for pushing updates to the Autofill Assistant UI for QR Code
 * Permission. These updates are pulled from the {@link AssistantQrCodePermissionModel} when a
 * notification of an update is received.
 */
class AssistantQrCodePermissionBinder implements ViewBinder<AssistantQrCodePermissionModel,
        AssistantQrCodePermissionBinder.ViewHolder, PropertyKey> {
    /**
     * A wrapper class that holds the different permission views of the QR Code UI.
     */
    static class ViewHolder {
        private final AssistantQrCodePermissionView mQrCodePermissionView;
        private final TextView mPermissionTextView;
        private final ButtonCompat mPermissionButton;

        public ViewHolder(AssistantQrCodePermissionView qrCodePermisionView) {
            mQrCodePermissionView = qrCodePermisionView;
            mPermissionTextView = qrCodePermisionView.getPermissionTextView();
            mPermissionButton = qrCodePermisionView.getPermissionButton();
        }
    }

    @Override
    public void bind(
            AssistantQrCodePermissionModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        if (propertyKey == AssistantQrCodePermissionModel.HAS_PERMISSION) {
            viewHolder.mQrCodePermissionView.onHasPermissionChanged(
                    model.get(AssistantQrCodePermissionModel.HAS_PERMISSION));
        } else if (propertyKey == AssistantQrCodePermissionModel.CAN_PROMPT_FOR_PERMISSION) {
            // When canPrompt value changes, we need to render different view. If canPrompt is true,
            // we can ask for permissions. Else, we need to tell users to open settings. Hence,
            // changing relevant title text and button text.
            if (model.get(AssistantQrCodePermissionModel.CAN_PROMPT_FOR_PERMISSION)) {
                viewHolder.mPermissionTextView.setText(
                        model.get(AssistantQrCodePermissionModel.PERMISSION_TEXT));
                viewHolder.mPermissionButton.setText(
                        model.get(AssistantQrCodePermissionModel.PERMISSION_BUTTON_TEXT));
            } else {
                viewHolder.mPermissionTextView.setText(
                        model.get(AssistantQrCodePermissionModel.OPEN_SETTINGS_TEXT));
                viewHolder.mPermissionButton.setText(
                        model.get(AssistantQrCodePermissionModel.OPEN_SETTINGS_BUTTON_TEXT));
            }
            viewHolder.mQrCodePermissionView.canPromptForPermissionChanged(
                    model.get(AssistantQrCodePermissionModel.CAN_PROMPT_FOR_PERMISSION));
        } else if (propertyKey == AssistantQrCodePermissionModel.PERMISSION_TEXT) {
            if (model.get(AssistantQrCodePermissionModel.CAN_PROMPT_FOR_PERMISSION)) {
                viewHolder.mPermissionTextView.setText(
                        model.get(AssistantQrCodePermissionModel.PERMISSION_TEXT));
            }
        } else if (propertyKey == AssistantQrCodePermissionModel.PERMISSION_BUTTON_TEXT) {
            if (model.get(AssistantQrCodePermissionModel.CAN_PROMPT_FOR_PERMISSION)) {
                viewHolder.mPermissionButton.setText(
                        model.get(AssistantQrCodePermissionModel.PERMISSION_BUTTON_TEXT));
            }
        } else if (propertyKey == AssistantQrCodePermissionModel.OPEN_SETTINGS_TEXT) {
            if (!model.get(AssistantQrCodePermissionModel.CAN_PROMPT_FOR_PERMISSION)) {
                viewHolder.mPermissionTextView.setText(
                        model.get(AssistantQrCodePermissionModel.OPEN_SETTINGS_TEXT));
            }
        } else if (propertyKey == AssistantQrCodePermissionModel.OPEN_SETTINGS_BUTTON_TEXT) {
            if (!model.get(AssistantQrCodePermissionModel.CAN_PROMPT_FOR_PERMISSION)) {
                viewHolder.mPermissionButton.setText(
                        model.get(AssistantQrCodePermissionModel.OPEN_SETTINGS_BUTTON_TEXT));
            }
        } else {
            assert false : "Unhandled property detected in AssistantQrCodePermissionBinder!";
        }
    }
}