// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.content.Context;
import android.content.res.ColorStateList;
import android.text.TextUtils;

import androidx.core.content.res.ResourcesCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.modelutil.PropertyModel;

/** This class creates the model for the permission dialog custom view. */
@NullMarked
class PermissionDialogCustomViewModelFactory {
    public static PropertyModel getModel(PermissionDialogDelegate delegate) {
        Context context = delegate.getWindow().getContext().get();
        assert context != null;

        String messageText = delegate.getMessageText();

        // TODO(crbug.com/388407665): we might change the assert when creating new UI.
        assert !TextUtils.isEmpty(messageText) || delegate.isEmbeddedPromptVariant();
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
                        ColorStateList.valueOf(
                                SemanticColorUtils.getDefaultIconColorOnAccent1Container(context)))
                .with(
                        PermissionDialogCustomViewProperties.CLOSE_BUTTON_CALLBACK,
                        delegate::onCloseButtonClicked)
                .build();
    }
}
