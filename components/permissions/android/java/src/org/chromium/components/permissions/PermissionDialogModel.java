// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.core.widget.TextViewCompat;

import org.chromium.components.browser_ui.modaldialog.R;
import org.chromium.ui.LayoutInflaterUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class creates the model for permission dialog.
 */
class PermissionDialogModel {
    public static PropertyModel getModel(ModalDialogProperties.Controller controller,
            PermissionDialogDelegate delegate, Runnable touchFilteredCallback) {
        Context context = delegate.getWindow().getContext().get();
        assert context != null;
        View customView = loadDialogView(context);

        String messageText = delegate.getMessageText();
        assert !TextUtils.isEmpty(messageText);

        TextView messageTextView = customView.findViewById(R.id.text);
        messageTextView.setText(messageText);
        TextViewCompat.setCompoundDrawablesRelativeWithIntrinsicBounds(
                messageTextView, delegate.getDrawableId(), 0, 0, 0);

        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, controller)
                .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, delegate.getPrimaryButtonText())
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, delegate.getSecondaryButtonText())
                .with(ModalDialogProperties.CONTENT_DESCRIPTION, delegate.getMessageText())
                .with(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY, true)
                .with(ModalDialogProperties.TOUCH_FILTERED_CALLBACK, touchFilteredCallback)
                .build();
    }

    private static View loadDialogView(Context context) {
        return LayoutInflaterUtils.inflate(context, R.layout.permission_dialog, null);
    }
}
