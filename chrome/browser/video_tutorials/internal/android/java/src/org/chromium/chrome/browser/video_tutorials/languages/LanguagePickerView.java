// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.languages;

import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * The View component of the language picker UI which contains the recycler view and the outer
 * layout.
 */
class LanguagePickerView {
    private final PropertyModel mModel;
    private final MVCListAdapter.ModelList mListModel;
    private final View mView;
    private final PropertyModelChangeProcessor<PropertyModel, View, PropertyKey>
            mPropertyModelChangeProcessor;

    /**
     * Constructor.
     * @param view The associated view.
     * @param model The model associated with the outer layout.
     * @param listModel The model associated with the list view.
     */
    public LanguagePickerView(View view, PropertyModel model, MVCListAdapter.ModelList listModel) {
        mView = view;
        mModel = model;
        mListModel = listModel;

        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mModel, mView, LanguagePickerView::bind);
        RecyclerView recyclerView = mView.findViewById(R.id.recycler_view);
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(mView.getContext(), LinearLayoutManager.VERTICAL, false);
        recyclerView.setLayoutManager(layoutManager);
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(mListModel);
        adapter.registerType(LanguageItemProperties.ITEM_VIEW_TYPE,
                LanguageItemViewHolder::buildView, LanguageItemViewHolder::bindView);
        recyclerView.setAdapter(adapter);
    }

    /** @return The Android {@link View} representing this widget. */
    public View getView() {
        return mView;
    }

    /**
     * The view binder to bind the model with the outer layout.
     */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == LanguagePickerProperties.WATCH_CALLBACK) {
            View watchButton = view.findViewById(R.id.watch);
            watchButton.setOnClickListener(
                    v -> { model.get(LanguagePickerProperties.WATCH_CALLBACK).run(); });
        } else if (propertyKey == LanguagePickerProperties.CLOSE_CALLBACK) {
            View closeButton = view.findViewById(R.id.close_button);
            closeButton.setOnClickListener(
                    v -> { model.get(LanguagePickerProperties.CLOSE_CALLBACK).run(); });
        } else if (propertyKey == LanguagePickerProperties.IS_ENABLED_WATCH_BUTTON) {
            View watchButton = view.findViewById(R.id.watch);
            watchButton.setEnabled(model.get(LanguagePickerProperties.IS_ENABLED_WATCH_BUTTON));
        }
    }
}
