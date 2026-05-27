// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.util.Pair;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.answer.AnswerSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionItemViewBuilder;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.entity.EntitySuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.groupseparator.GroupSeparatorView;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderView;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.tail.TailSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.tail.TailSuggestionViewBinder;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.MVCListAdapter.ViewBuilder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Factory class responsible for creating ViewHolders for Omnibox suggestions. */
@NullMarked
public class OmniboxViewHolderFactory {
    private final SparseArray<Pair<ViewBuilder, ViewBinder>> mViewBuilderMap = new SparseArray<>();

    private final RecyclerView.Adapter<SimpleRecyclerViewAdapter.ViewHolder> mAdapter =
            new RecyclerView.Adapter<>() {
                @SuppressWarnings("unchecked")
                @Override
                public SimpleRecyclerViewAdapter.ViewHolder onCreateViewHolder(
                        ViewGroup parent, int viewType) {
                    return createViewHolderForType(parent, viewType);
                }

                @Override
                public void onBindViewHolder(
                        SimpleRecyclerViewAdapter.ViewHolder holder, int position) {}

                @Override
                public int getItemCount() {
                    return 0;
                }
            };

    public OmniboxViewHolderFactory() {
        registerType(
                OmniboxSuggestionUiType.DEFAULT,
                parent ->
                        new BaseSuggestionView<>(
                                parent.getContext(), R.layout.omnibox_basic_suggestion),
                new BaseSuggestionViewBinder<>(SuggestionViewViewBinder::bind));

        registerType(
                OmniboxSuggestionUiType.EDIT_URL_SUGGESTION,
                parent ->
                        new BaseSuggestionView<>(
                                parent.getContext(), R.layout.omnibox_basic_suggestion),
                new BaseSuggestionViewBinder<>(SuggestionViewViewBinder::bind));

        registerType(
                OmniboxSuggestionUiType.ANSWER_SUGGESTION,
                parent ->
                        new BaseSuggestionView<>(
                                parent.getContext(), R.layout.omnibox_answer_suggestion),
                new BaseSuggestionViewBinder<>(AnswerSuggestionViewBinder::bind));

        registerType(
                OmniboxSuggestionUiType.ENTITY_SUGGESTION,
                parent ->
                        new BaseSuggestionView<>(
                                parent.getContext(), R.layout.omnibox_basic_suggestion),
                new BaseSuggestionViewBinder<>(EntitySuggestionViewBinder::bind));

        registerType(
                OmniboxSuggestionUiType.TAIL_SUGGESTION,
                parent -> new BaseSuggestionView<>(new TailSuggestionView(parent.getContext())),
                new BaseSuggestionViewBinder<>(TailSuggestionViewBinder::bind));

        registerType(
                OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION,
                parent ->
                        new BaseSuggestionView<>(
                                parent.getContext(), R.layout.omnibox_basic_suggestion),
                new BaseSuggestionViewBinder<>(SuggestionViewViewBinder::bind));

        registerType(
                OmniboxSuggestionUiType.TAB_GROUP_SUGGESTION,
                parent ->
                        new BaseSuggestionView<>(
                                parent.getContext(), R.layout.omnibox_basic_suggestion),
                new BaseSuggestionViewBinder<>(SuggestionViewViewBinder::bind));

        registerType(
                OmniboxSuggestionUiType.TILE_NAVSUGGEST,
                BaseCarouselSuggestionItemViewBuilder::createView,
                BaseCarouselSuggestionViewBinder::bind);

        registerType(
                OmniboxSuggestionUiType.HEADER,
                parent -> new HeaderView(parent.getContext()),
                HeaderViewBinder::bind);

        registerType(
                OmniboxSuggestionUiType.GROUP_SEPARATOR,
                parent -> new GroupSeparatorView(parent.getContext()),
                (m, v, p) -> {});
    }

    private <T extends View> void registerType(
            int typeId, ViewBuilder<T> builder, ViewBinder<PropertyModel, T, PropertyKey> binder) {
        assert mViewBuilderMap.get(typeId) == null;
        mViewBuilderMap.put(typeId, new Pair<>(builder, binder));
    }

    @SuppressWarnings("unchecked")
    protected SimpleRecyclerViewAdapter.ViewHolder createViewHolderForType(
            ViewGroup parent, int viewType) {
        try (TraceEvent tracing =
                        TraceEvent.scoped("OmniboxSuggestionsList.CreateView", "type:" + viewType);
                TimingMetric metric = OmniboxMetrics.recordSuggestionViewCreateTime();
                TimingMetric metric2 = OmniboxMetrics.recordSuggestionViewCreateWallTime()) {
            var pair = assumeNonNull(mViewBuilderMap.get(viewType));
            View view = pair.first.buildView(parent);
            return new SimpleRecyclerViewAdapter.ViewHolder(view, pair.second);
        }
    }

    public View createView(ViewGroup parent, int viewType) {
        return assumeNonNull(mViewBuilderMap.get(viewType)).first.buildView(parent);
    }

    public SimpleRecyclerViewAdapter.ViewHolder createViewHolderForAdapter(
            ViewGroup parent, int viewType) {
        return createViewHolderForType(parent, viewType);
    }

    public SimpleRecyclerViewAdapter.ViewHolder createViewHolderForPool(
            ViewGroup parent, int viewType) {
        return mAdapter.createViewHolder(parent, viewType);
    }
}
