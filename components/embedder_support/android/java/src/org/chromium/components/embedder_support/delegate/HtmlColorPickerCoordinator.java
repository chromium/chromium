// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.delegate;

import static org.chromium.components.embedder_support.delegate.HtmlColorPickerProperties.CHOSEN_COLOR;
import static org.chromium.components.embedder_support.delegate.HtmlColorPickerProperties.CHOSEN_SUGGESTION_INDEX;
import static org.chromium.components.embedder_support.delegate.HtmlColorPickerProperties.CUSTOM_COLOR_PICKED_CALLBACK;
import static org.chromium.components.embedder_support.delegate.HtmlColorPickerProperties.DIALOG_DISMISSED_CALLBACK;
import static org.chromium.components.embedder_support.delegate.HtmlColorPickerProperties.IS_ADVANCED_VIEW;
import static org.chromium.components.embedder_support.delegate.HtmlColorPickerProperties.MAKE_CHOICE_CALLBACK;
import static org.chromium.components.embedder_support.delegate.HtmlColorPickerProperties.SUGGESTIONS_ADAPTER;
import static org.chromium.components.embedder_support.delegate.HtmlColorPickerProperties.SUGGESTIONS_NUM_COLUMNS;
import static org.chromium.components.embedder_support.delegate.HtmlColorPickerProperties.VIEW_SWITCHED_CALLBACK;

import android.content.Context;
import android.graphics.Color;

import org.chromium.base.Callback;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
@NullMarked
public class HtmlColorPickerCoordinator {
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
    private final HtmlColorPickerDialogView mHtmlColorPickerDialogView;
    private final List<HtmlColorSuggestion> mSuggestions;
    private PropertyModel mModel;
    private MVCListAdapter.ModelList mSuggestionsModelList;
    private ModelListAdapter mSuggestionsAdapter;

    static HtmlColorPickerCoordinator create(
            Context context, Callback<Integer> dialogDismissedCallback) {
        HtmlColorPickerDialogView dialogView = new HtmlColorPickerDialogView(context);
        return new HtmlColorPickerCoordinator(context, dialogDismissedCallback, dialogView);
    }

    public HtmlColorPickerCoordinator(
            Context context,
            Callback<Integer> dialogDismissedCallback,
            HtmlColorPickerDialogView dialogView) {
        mContext = context;
        mDialogDismissedCallback = dialogDismissedCallback;
        mSuggestions = new ArrayList<>();
        mHtmlColorPickerDialogView = dialogView;
    }

    @Initializer
    void show(int initialColor) {
        mInitialColor = initialColor;

        if (mSuggestions.isEmpty()) {
            createDefaultSuggestions();
        }

        // Construct a ModelListAdapter for the given suggestions.
        generateSuggestionsModelList();
        mSuggestionsAdapter = new ModelListAdapter(mSuggestionsModelList);
        mSuggestionsAdapter.registerType(
                HtmlColorPickerSuggestionProperties.ListItemType.DEFAULT,
                HtmlColorPickerViewBinder::buildView,
                HtmlColorPickerViewBinder::bindAdapter);

        mModel =
                new PropertyModel.Builder(HtmlColorPickerProperties.ALL_KEYS)
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
                mModel, mHtmlColorPickerDialogView, HtmlColorPickerViewBinder::bind);

        mHtmlColorPickerDialogView.show();
        DialogTypeRecorder.recordDialogType(DialogTypeRecorder.DialogType.COLOR_PICKER);
    }

    void close() {
        mHtmlColorPickerDialogView.dismiss();
    }

    public void addColorSuggestion(int color, String label) {
        mSuggestions.add(new HtmlColorSuggestion(color, label));
    }

    private void createDefaultSuggestions() {
        assert mSuggestions.isEmpty();
        assert DEFAULT_COLORS.length == DEFAULT_COLOR_LABEL_IDS.length;
        for (int i = 0; i < DEFAULT_COLORS.length; i++) {
            mSuggestions.add(
                    new HtmlColorSuggestion(
                            DEFAULT_COLORS[i], mContext.getString(DEFAULT_COLOR_LABEL_IDS[i])));
        }
    }

    private void generateSuggestionsModelList() {
        assert !mSuggestions.isEmpty();
        mSuggestionsModelList = new MVCListAdapter.ModelList();
        for (int i = 0; i < mSuggestions.size(); i++) {
            HtmlColorSuggestion suggestion = mSuggestions.get(i);
            PropertyModel itemModel =
                    new PropertyModel.Builder(HtmlColorPickerSuggestionProperties.ALL_KEYS)
                            .with(HtmlColorPickerSuggestionProperties.INDEX, i)
                            .with(HtmlColorPickerSuggestionProperties.COLOR, suggestion.mColor)
                            .with(HtmlColorPickerSuggestionProperties.LABEL, suggestion.mLabel)
                            .with(HtmlColorPickerSuggestionProperties.IS_SELECTED, false)
                            .with(
                                    HtmlColorPickerSuggestionProperties.ONCLICK,
                                    this::handleSuggestionColorPicked)
                            .build();
            mSuggestionsModelList.add(
                    new MVCListAdapter.ListItem(
                            HtmlColorPickerSuggestionProperties.ListItemType.DEFAULT, itemModel));
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
                    .set(HtmlColorPickerSuggestionProperties.IS_SELECTED, false);
        }
        mSuggestionsModelList
                .get(index)
                .model
                .set(HtmlColorPickerSuggestionProperties.IS_SELECTED, true);
        mModel.set(CHOSEN_SUGGESTION_INDEX, index);
        mModel.set(
                CHOSEN_COLOR,
                mSuggestionsModelList
                        .get(index)
                        .model
                        .get(HtmlColorPickerSuggestionProperties.COLOR));
    }

    private void handleCustomColorPicked(int newColor) {
        // Set chosen suggestion index back to default value of -1 if present, and update the
        // IS_SELECTED value of whatever suggestion was picked (if present).
        if (mModel.get(CHOSEN_SUGGESTION_INDEX) != -1) {
            mSuggestionsModelList
                    .get(mModel.get(CHOSEN_SUGGESTION_INDEX))
                    .model
                    .set(HtmlColorPickerSuggestionProperties.IS_SELECTED, false);
            mModel.set(CHOSEN_SUGGESTION_INDEX, -1);
        }
        mModel.set(CHOSEN_COLOR, newColor);
    }

    private void handleViewSwitched(@Nullable Void unused) {
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
