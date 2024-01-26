// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.StyleSpan;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.util.Pair;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** The {@View} binder class for the PermissionDialogCustomView MVC. */
class PermissionOneTimeDialogCustomViewBinder {
    public static void bind(PropertyModel model, View customView, PropertyKey propertyKey) {
        if (PermissionDialogCustomViewProperties.MESSAGE_TEXT == propertyKey) {
            assert model.get(PermissionDialogCustomViewProperties.MESSAGE_TEXT) != null;
            List<Pair<Integer, Integer>> boldedRanges = new ArrayList<>();
            if (model.containsKey(PermissionDialogCustomViewProperties.BOLDED_RANGES)
                    && model.get(PermissionDialogCustomViewProperties.BOLDED_RANGES) != null) {
                boldedRanges.addAll(model.get(PermissionDialogCustomViewProperties.BOLDED_RANGES));
            }
            updateMessageText(
                    customView,
                    model.get(PermissionDialogCustomViewProperties.MESSAGE_TEXT),
                    boldedRanges);
        } else if (PermissionDialogCustomViewProperties.ICON == propertyKey) {
            assert model.get(PermissionDialogCustomViewProperties.ICON) != null;
            updateIcon(customView, model.get(PermissionDialogCustomViewProperties.ICON));
        } else if (PermissionDialogCustomViewProperties.ICON_TINT == propertyKey) {
            updateTintColor(customView, model.get(PermissionDialogCustomViewProperties.ICON_TINT));
        }
    }

    private static void updateMessageText(
            @NonNull View customView,
            @NonNull String messageText,
            List<Pair<Integer, Integer>> boldedRanges) {
        TextView messageTextView = customView.findViewById(R.id.text);
        final SpannableStringBuilder sb = new SpannableStringBuilder(messageText);
        final StyleSpan bss = new StyleSpan(android.graphics.Typeface.BOLD);
        boldedRanges.forEach(
                boldRange -> {
                    sb.setSpan(
                            bss,
                            boldRange.first,
                            boldRange.second,
                            Spannable.SPAN_INCLUSIVE_INCLUSIVE);
                });
        messageTextView.setText(sb);
    }

    private static void updateIcon(@NonNull View customView, @NonNull Drawable icon) {
        ImageView iconView = customView.findViewById(R.id.icon);
        iconView.setImageDrawable(icon);
    }

    private static void updateTintColor(
            @NonNull View customView, @Nullable ColorStateList iconTint) {
        ImageView iconView = customView.findViewById(R.id.icon);
        iconView.setImageTintList(iconTint);
    }
}
