// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.content.Context;
import android.text.TextUtils;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.ui.modelutil.PropertyModel;

/** This class creates the model for the permission dialog custom view. */
class PermissionDialogCustomViewModelFactory {
    public static PropertyModel getModel(PermissionDialogDelegate delegate) {
        Context context = delegate.getWindow().getContext().get();
        assert context != null;

        String messageText = delegate.getMessageText();
        assert !TextUtils.isEmpty(messageText);

        return new PropertyModel.Builder(PermissionDialogCustomViewProperties.ALL_KEYS)
                .with(PermissionDialogCustomViewProperties.MESSAGE_TEXT, messageText)
                .with(
                        PermissionDialogCustomViewProperties.BOLDED_RANGES,
                        delegate.getBoldedRanges())
                .with(
                        PermissionDialogCustomViewProperties.ICON,
                        ResourcesCompat.getDrawable(
                                context.getResources(),
                                delegate.getDrawableId(),
                                context.getTheme()))
                .with(
                        PermissionDialogCustomViewProperties.ICON_TINT,
                        AppCompatResources.getColorStateList(
                                context, R.color.default_icon_color_accent1_tint_list))
                .build();
    }
}
