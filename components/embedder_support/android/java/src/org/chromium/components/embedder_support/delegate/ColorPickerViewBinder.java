// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.delegate;

import static org.chromium.components.embedder_support.delegate.ColorPickerProperties.CHOSEN_COLOR;
import static org.chromium.components.embedder_support.delegate.ColorPickerProperties.CUSTOM_COLOR_PICKED_CALLBACK;
import static org.chromium.components.embedder_support.delegate.ColorPickerProperties.DIALOG_DISMISSED_CALLBACK;
import static org.chromium.components.embedder_support.delegate.ColorPickerProperties.IS_ADVANCED_VIEW;
import static org.chromium.components.embedder_support.delegate.ColorPickerProperties.MAKE_CHOICE_CALLBACK;
import static org.chromium.components.embedder_support.delegate.ColorPickerProperties.SUGGESTIONS_ADAPTER;
import static org.chromium.components.embedder_support.delegate.ColorPickerProperties.SUGGESTIONS_NUM_COLUMNS;
import static org.chromium.components.embedder_support.delegate.ColorPickerProperties.VIEW_SWITCHED_CALLBACK;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class is to call the methods from the view classes and to paint them based on the current
 * properties.
 */
public class ColorPickerViewBinder {
    public static void bind(
            PropertyModel model, ColorPickerDialogView dialogView, PropertyKey propertyKey) {
        if (CHOSEN_COLOR == propertyKey) {
            dialogView.setColor(model.get(CHOSEN_COLOR));
        } else if (SUGGESTIONS_NUM_COLUMNS == propertyKey) {
            dialogView.setNumberOfColumns(model.get(SUGGESTIONS_NUM_COLUMNS));
        } else if (SUGGESTIONS_ADAPTER == propertyKey) {
            dialogView.setSuggestionsAdapter(model.get(SUGGESTIONS_ADAPTER));
        } else if (IS_ADVANCED_VIEW == propertyKey) {
            dialogView.switchViewType(model.get(IS_ADVANCED_VIEW));
        } else if (CUSTOM_COLOR_PICKED_CALLBACK == propertyKey) {
            dialogView.setCustomColorPickedCallback(model.get(CUSTOM_COLOR_PICKED_CALLBACK));
        } else if (VIEW_SWITCHED_CALLBACK == propertyKey) {
            dialogView.setViewSwitchedCallback(model.get(VIEW_SWITCHED_CALLBACK));
        } else if (MAKE_CHOICE_CALLBACK == propertyKey) {
            dialogView.setMakeChoiceCallback(model.get(MAKE_CHOICE_CALLBACK));
        } else if (DIALOG_DISMISSED_CALLBACK == propertyKey) {
            dialogView.setDialogDismissedCallback(model.get(DIALOG_DISMISSED_CALLBACK));
        }
    }

    public static View buildView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.color_picker_suggestion_view, parent, false);
    }

    public static void bindAdapter(
            PropertyModel model, View suggestionView, PropertyKey propertyKey) {
        if (ColorPickerSuggestionProperties.COLOR == propertyKey) {
            final View colorSuggestion =
                    suggestionView.findViewById(R.id.color_picker_suggestion_color_view);
            colorSuggestion.setBackgroundColor(model.get(ColorPickerSuggestionProperties.COLOR));
        } else if (ColorPickerSuggestionProperties.ONCLICK == propertyKey) {
            suggestionView.setOnClickListener(
                    v ->
                            model.get(ColorPickerSuggestionProperties.ONCLICK)
                                    .onResult(model.get(ColorPickerSuggestionProperties.INDEX)));
        } else {
            suggestionView.setAccessibilityDelegate(
                    new View.AccessibilityDelegate() {
                        @Override
                        public void onInitializeAccessibilityNodeInfo(
                                View host, AccessibilityNodeInfo info) {
                            super.onInitializeAccessibilityNodeInfo(host, info);
                            info.setCollectionItemInfo(
                                    AccessibilityNodeInfo.CollectionItemInfo.obtain(
                                            model.get(ColorPickerSuggestionProperties.INDEX),
                                            1,
                                            1,
                                            1,
                                            false));
                            info.setSelected(
                                    model.get(ColorPickerSuggestionProperties.IS_SELECTED));
                            info.setText(model.get(ColorPickerSuggestionProperties.LABEL));
                        }
                    });
        }
    }

    private ColorPickerViewBinder() {}
}
