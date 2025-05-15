// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.StyleSpan;
import android.view.View;
import android.widget.ImageView;
import android.widget.RadioGroup;
import android.widget.TextView;

import androidx.core.util.Pair;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.RadioButtonLayout;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** The {@View} binder class for the PermissionDialogCustomView MVC. */
@NullMarked
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
        } else if (PermissionDialogCustomViewProperties.RADIO_BUTTONS == propertyKey) {
            updateRadioButtons(
                    customView, model.get(PermissionDialogCustomViewProperties.RADIO_BUTTONS));
        } else if (PermissionDialogCustomViewProperties.RADIO_BUTTON_CALLBACK == propertyKey) {
            updateRadioButtonCallback(
                    customView,
                    model.get(PermissionDialogCustomViewProperties.RADIO_BUTTON_CALLBACK));
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

    private static void updateRadioButtons(View customView, List<CharSequence> radioButtonTexts) {
        RadioButtonLayout radioButtons = customView.findViewById(R.id.radio_buttons);
        if (radioButtons.getChildCount() > 0) {
            radioButtons.removeAllViews();
        }
        radioButtons.setVisibility(radioButtonTexts.isEmpty() ? GONE : VISIBLE);
        radioButtons.addOptions(radioButtonTexts, null);
    }

    private static void updateRadioButtonCallback(
            View customView, Callback<Integer> radioButtonCallback) {
        RadioGroup radioButtons = customView.findViewById(R.id.radio_buttons);
        radioButtons.setOnCheckedChangeListener(
                (view, id) -> {
                    radioButtonCallback.onResult(view.indexOfChild(view.findViewById(id)));
                });
    }
}
