// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.delegate;

import static org.chromium.components.embedder_support.delegate.ColorPickerProperties.CHOSEN_COLOR;
import static org.chromium.components.embedder_support.delegate.ColorPickerProperties.CHOSEN_SUGGESTION_INDEX;
import static org.chromium.components.embedder_support.delegate.ColorPickerProperties.CUSTOM_COLOR_PICKED_CALLBACK;
import static org.chromium.components.embedder_support.delegate.ColorPickerProperties.DIALOG_DISMISSED_CALLBACK;
import static org.chromium.components.embedder_support.delegate.ColorPickerProperties.IS_ADVANCED_VIEW;
import static org.chromium.components.embedder_support.delegate.ColorPickerProperties.MAKE_CHOICE_CALLBACK;
import static org.chromium.components.embedder_support.delegate.ColorPickerProperties.SUGGESTIONS_ADAPTER;
import static org.chromium.components.embedder_support.delegate.ColorPickerProperties.SUGGESTIONS_NUM_COLUMNS;
import static org.chromium.components.embedder_support.delegate.ColorPickerProperties.VIEW_SWITCHED_CALLBACK;

import android.content.Context;
import android.graphics.Color;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.content_public.browser.util.DialogTypeRecorder;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * This class enables the creation of the color picker dialog and make the decisions within the
 * component. Apart from calculating and creating the columns and creating the suggestion colors, it
 * also handles the switches between simple and advanced views.
 */
public class ColorPickerCoordinator {
    // Default color suggestions, overridden if a single suggestion is provided by the web.
    private static final int[] DEFAULT_COLORS = {
        Color.RED,
        Color.CYAN,
        Color.BLUE,
        Color.GREEN,
        Color.MAGENTA,
        Color.YELLOW,
        Color.BLACK,
        Color.WHITE
    };

    private static final int[] DEFAULT_COLOR_LABEL_IDS = {
        R.string.color_picker_button_red,
        R.string.color_picker_button_cyan,
        R.string.color_picker_button_blue,
        R.string.color_picker_button_green,
        R.string.color_picker_button_magenta,
        R.string.color_picker_button_yellow,
        R.string.color_picker_button_black,
        R.string.color_picker_button_white
    };

    private static final int MAX_NUMBER_OF_COLUMNS = 5;

    private int mInitialColor;
    private final Context mContext;
    private final Callback<Integer> mDialogDismissedCallback;
    private final ColorPickerDialogView mColorPickerDialogView;
    private List<ColorSuggestion> mSuggestions;
    private PropertyModel mModel;

    private MVCListAdapter.ModelList mSuggestionsModelList;
    private ModelListAdapter mSuggestionsAdapter;

    static ColorPickerCoordinator create(
            @NonNull Context context, Callback<Integer> dialogDismissedCallback) {
        ColorPickerDialogView dialogView = new ColorPickerDialogView(context);
        return new ColorPickerCoordinator(context, dialogDismissedCallback, dialogView);
    }

    public ColorPickerCoordinator(
            @NonNull Context context,
            @NonNull Callback<Integer> dialogDismissedCallback,
            @NonNull ColorPickerDialogView dialogView) {
        mContext = context;
        mDialogDismissedCallback = dialogDismissedCallback;
        mSuggestions = new ArrayList<>();
        mColorPickerDialogView = dialogView;
    }

    void show(int initialColor) {
        mInitialColor = initialColor;

        if (mSuggestions.isEmpty()) {
            createDefaultSuggestions();
        }

        // Construct a ModelListAdapter for the given suggestions.
        generateSuggestionsModelList();
        mSuggestionsAdapter = new ModelListAdapter(mSuggestionsModelList);
        mSuggestionsAdapter.registerType(
                ColorPickerSuggestionProperties.ListItemType.DEFAULT,
                ColorPickerViewBinder::buildView,
                ColorPickerViewBinder::bindAdapter);

        mModel =
                new PropertyModel.Builder(ColorPickerProperties.ALL_KEYS)
                        .with(CHOSEN_COLOR, initialColor)
                        .with(CHOSEN_SUGGESTION_INDEX, -1)
                        .with(SUGGESTIONS_NUM_COLUMNS, calculateNumberOfColumns())
                        .with(SUGGESTIONS_ADAPTER, mSuggestionsAdapter)
                        .with(IS_ADVANCED_VIEW, false)
                        .with(CUSTOM_COLOR_PICKED_CALLBACK, this::handleCustomColorPicked)
                        .with(VIEW_SWITCHED_CALLBACK, this::handleViewSwitched)
                        .with(MAKE_CHOICE_CALLBACK, this::handleMakeChoice)
                        .with(DIALOG_DISMISSED_CALLBACK, mDialogDismissedCallback)
                        .build();

        PropertyModelChangeProcessor.create(
                mModel, mColorPickerDialogView, ColorPickerViewBinder::bind);

        mColorPickerDialogView.show();
        DialogTypeRecorder.recordDialogType(DialogTypeRecorder.DialogType.COLOR_PICKER);
    }

    void close() {
        mColorPickerDialogView.dismiss();
    }

    public void addColorSuggestion(int color, String label) {
        mSuggestions.add(new ColorSuggestion(color, label));
    }

    private void createDefaultSuggestions() {
        assert mSuggestions.isEmpty();
        assert DEFAULT_COLORS.length == DEFAULT_COLOR_LABEL_IDS.length;
        for (int i = 0; i < DEFAULT_COLORS.length; i++) {
            mSuggestions.add(
                    new ColorSuggestion(
                            DEFAULT_COLORS[i], mContext.getString(DEFAULT_COLOR_LABEL_IDS[i])));
        }
    }

    private void generateSuggestionsModelList() {
        assert !mSuggestions.isEmpty();
        mSuggestionsModelList = new MVCListAdapter.ModelList();
        for (int i = 0; i < mSuggestions.size(); i++) {
            ColorSuggestion suggestion = mSuggestions.get(i);
            PropertyModel itemModel =
                    new PropertyModel.Builder(ColorPickerSuggestionProperties.ALL_KEYS)
                            .with(ColorPickerSuggestionProperties.INDEX, i)
                            .with(ColorPickerSuggestionProperties.COLOR, suggestion.mColor)
                            .with(ColorPickerSuggestionProperties.LABEL, suggestion.mLabel)
                            .with(ColorPickerSuggestionProperties.IS_SELECTED, false)
                            .with(
                                    ColorPickerSuggestionProperties.ONCLICK,
                                    this::handleSuggestionColorPicked)
                            .build();
            mSuggestionsModelList.add(
                    new MVCListAdapter.ListItem(
                            ColorPickerSuggestionProperties.ListItemType.DEFAULT, itemModel));
        }
    }

    private int calculateNumberOfColumns() {
        assert !mSuggestions.isEmpty();
        if (mSuggestions.size() <= MAX_NUMBER_OF_COLUMNS) {
            return mSuggestions.size();
        } else {
            return Math.min(
                    MAX_NUMBER_OF_COLUMNS,
                    // Since we want to show two rows of equal number of columns,
                    // we need an even number. This calculation is for if
                    // mSuggestions.size() is an uneven number,
                    (int) mSuggestions.size() / 2 + (mSuggestions.size() % 2));
        }
    }

    private void handleSuggestionColorPicked(int index) {
        // Remove previous selection if present.
        if (mModel.get(CHOSEN_SUGGESTION_INDEX) != -1) {
            mSuggestionsModelList
                    .get(mModel.get(CHOSEN_SUGGESTION_INDEX))
                    .model
                    .set(ColorPickerSuggestionProperties.IS_SELECTED, false);
        }
        mSuggestionsModelList
                .get(index)
                .model
                .set(ColorPickerSuggestionProperties.IS_SELECTED, true);
        mModel.set(CHOSEN_SUGGESTION_INDEX, index);
        mModel.set(
                CHOSEN_COLOR,
                mSuggestionsModelList.get(index).model.get(ColorPickerSuggestionProperties.COLOR));
    }

    private void handleCustomColorPicked(int newColor) {
        // Set chosen suggestion index back to default value of -1 if present, and update the
        // IS_SELECTED value of whatever suggestion was picked (if present).
        if (mModel.get(CHOSEN_SUGGESTION_INDEX) != -1) {
            mSuggestionsModelList
                    .get(mModel.get(CHOSEN_SUGGESTION_INDEX))
                    .model
                    .set(ColorPickerSuggestionProperties.IS_SELECTED, false);
            mModel.set(CHOSEN_SUGGESTION_INDEX, -1);
        }
        mModel.set(CHOSEN_COLOR, newColor);
    }

    private void handleViewSwitched(Void unused) {
        mModel.set(IS_ADVANCED_VIEW, !mModel.get(IS_ADVANCED_VIEW));
    }

    private void handleMakeChoice(boolean chosen) {
        if (chosen) {
            mDialogDismissedCallback.onResult(mModel.get(CHOSEN_COLOR));
        } else {
            mDialogDismissedCallback.onResult(mInitialColor);
        }
    }
}
