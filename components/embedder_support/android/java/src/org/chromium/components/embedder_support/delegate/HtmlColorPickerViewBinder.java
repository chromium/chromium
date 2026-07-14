// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.delegate;

import static org.chromium.components.embedder_support.delegate.HtmlColorPickerProperties.CHOSEN_COLOR;
import static org.chromium.components.embedder_support.delegate.HtmlColorPickerProperties.CUSTOM_COLOR_PICKED_CALLBACK;
import static org.chromium.components.embedder_support.delegate.HtmlColorPickerProperties.DIALOG_DISMISSED_CALLBACK;
import static org.chromium.components.embedder_support.delegate.HtmlColorPickerProperties.IS_ADVANCED_VIEW;
import static org.chromium.components.embedder_support.delegate.HtmlColorPickerProperties.MAKE_CHOICE_CALLBACK;
import static org.chromium.components.embedder_support.delegate.HtmlColorPickerProperties.SUGGESTIONS_ADAPTER;
import static org.chromium.components.embedder_support.delegate.HtmlColorPickerProperties.SUGGESTIONS_NUM_COLUMNS;
import static org.chromium.components.embedder_support.delegate.HtmlColorPickerProperties.VIEW_SWITCHED_CALLBACK;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class is to call the methods from the view classes and to paint them based on the current
 * properties.
 */
@NullMarked
public class HtmlColorPickerViewBinder {
    public static void bind(
            PropertyModel model, HtmlColorPickerDialogView dialogView, PropertyKey propertyKey) {
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
        if (HtmlColorPickerSuggestionProperties.COLOR == propertyKey) {
            final View colorSuggestion =
                    suggestionView.findViewById(R.id.color_picker_suggestion_color_view);
            colorSuggestion.setBackgroundColor(
                    model.get(HtmlColorPickerSuggestionProperties.COLOR));
        } else if (HtmlColorPickerSuggestionProperties.ONCLICK == propertyKey) {
            suggestionView.setOnClickListener(
                    v ->
                            model.get(HtmlColorPickerSuggestionProperties.ONCLICK)
                                    .onResult(
                                            model.get(HtmlColorPickerSuggestionProperties.INDEX)));
        } else {
            suggestionView.setContentDescription(
                    model.get(HtmlColorPickerSuggestionProperties.LABEL));
            suggestionView.setSelected(model.get(HtmlColorPickerSuggestionProperties.IS_SELECTED));
            suggestionView.setAccessibilityDelegate(
                    new View.AccessibilityDelegate() {
                        @Override
                        public void onInitializeAccessibilityNodeInfo(
                                View host, AccessibilityNodeInfo info) {
                            super.onInitializeAccessibilityNodeInfo(host, info);
                            info.setCollectionItemInfo(
                                    AccessibilityNodeInfo.CollectionItemInfo.obtain(
                                            model.get(HtmlColorPickerSuggestionProperties.INDEX),
                                            1,
                                            1,
                                            1,
                                            false));
                        }
                    });
        }
    }

    private HtmlColorPickerViewBinder() {}
}
