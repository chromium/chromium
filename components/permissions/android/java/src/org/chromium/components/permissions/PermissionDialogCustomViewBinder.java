// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The {@View} binder class for the PermissionDialogCustomView MVC. */
class PermissionDialogCustomViewBinder {
    public static void bind(PropertyModel model, View customView, PropertyKey propertyKey) {
        if (PermissionDialogCustomViewProperties.MESSAGE_TEXT == propertyKey) {
            assert model.get(PermissionDialogCustomViewProperties.MESSAGE_TEXT) != null;
            updateMessageText(
                    customView, model.get(PermissionDialogCustomViewProperties.MESSAGE_TEXT));
        } else if (PermissionDialogCustomViewProperties.ICON == propertyKey) {
            assert model.get(PermissionDialogCustomViewProperties.ICON) != null;
            updateIcon(customView, model.get(PermissionDialogCustomViewProperties.ICON));
        } else if (PermissionDialogCustomViewProperties.ICON_TINT == propertyKey) {
            updateTintColor(customView, model.get(PermissionDialogCustomViewProperties.ICON_TINT));
        }
    }

    private static void updateMessageText(@NonNull View customView, @NonNull String messageText) {
        TextView messageTextView = customView.findViewById(R.id.text);
        messageTextView.setText(messageText);
    }

    private static void updateIcon(@NonNull View customView, @NonNull Drawable icon) {
        TextViewWithCompoundDrawables messageTextView = customView.findViewById(R.id.text);
        messageTextView.setCompoundDrawablesRelativeWithIntrinsicBounds(icon, null, null, null);
    }

    private static void updateTintColor(
            @NonNull View customView, @Nullable ColorStateList iconTint) {
        TextViewWithCompoundDrawables messageTextView = customView.findViewById(R.id.text);
        messageTextView.setDrawableTintColor(iconTint);
    }
}
