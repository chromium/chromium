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

import androidx.core.util.Pair;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** The {@View} binder class for the PermissionDialogCustomView MVC. */
@NullMarked
class PermissionVerticalButtonsDialogCustomViewBinder {
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
        } else if (PermissionDialogCustomViewProperties.CLOSE_BUTTON_CALLBACK == propertyKey) {
            updateCloseButtonCallback(
                    customView,
                    model.get(PermissionDialogCustomViewProperties.CLOSE_BUTTON_CALLBACK));
        }
    }

    private static void updateMessageText(
            View customView, String messageText, List<Pair<Integer, Integer>> boldedRanges) {
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

    private static void updateIcon(View customView, Drawable icon) {
        ImageView iconView = customView.findViewById(R.id.icon);
        iconView.setImageDrawable(icon);
    }

    private static void updateTintColor(View customView, @Nullable ColorStateList iconTint) {
        ImageView iconView = customView.findViewById(R.id.icon);
        iconView.setImageTintList(iconTint);
    }

    private static void updateCloseButtonCallback(View customView, Runnable closeButtonCallback) {
        View closeButton = customView.findViewById(R.id.close_button);
        if (closeButton != null) {
            final View.OnClickListener callback =
                    (View view) -> {
                        closeButtonCallback.run();
                        closeButton.setOnClickListener(null);
                    };
            closeButton.setOnClickListener(callback);
        }
    }
}
