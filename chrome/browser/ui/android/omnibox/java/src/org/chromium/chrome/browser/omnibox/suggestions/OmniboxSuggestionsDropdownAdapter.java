// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.answer.AnswerSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionItemViewBuilder;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.entity.EntitySuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.groupseparator.GroupSeparatorView;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderView;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.tail.TailSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.tail.TailSuggestionViewBinder;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** ModelListAdapter for OmniboxSuggestionsDropdown (RecyclerView version). */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public class OmniboxSuggestionsDropdownAdapter extends SimpleRecyclerViewAdapter {
    private int mNumSessionViewsCreated;
    private int mNumSessionViewsBound;

    OmniboxSuggestionsDropdownAdapter(ModelList data) {
        super(data);

        // Register a view type for a default omnibox suggestion.
        registerType(
                OmniboxSuggestionUiType.DEFAULT,
                parent ->
                        new BaseSuggestionView<View>(
                                parent.getContext(), R.layout.omnibox_basic_suggestion),
                new BaseSuggestionViewBinder<View>(SuggestionViewViewBinder::bind));

        registerType(
                OmniboxSuggestionUiType.EDIT_URL_SUGGESTION,
                parent -> new EditUrlSuggestionView(parent.getContext()),
                new EditUrlSuggestionViewBinder());

        registerType(
                OmniboxSuggestionUiType.ANSWER_SUGGESTION,
                parent ->
                        new BaseSuggestionView<View>(
                                parent.getContext(), R.layout.omnibox_answer_suggestion),
                new BaseSuggestionViewBinder<View>(AnswerSuggestionViewBinder::bind));

        registerType(
                OmniboxSuggestionUiType.ENTITY_SUGGESTION,
                parent ->
                        new BaseSuggestionView<View>(
                                parent.getContext(), R.layout.omnibox_basic_suggestion),
                new BaseSuggestionViewBinder<View>(EntitySuggestionViewBinder::bind));

        registerType(
                OmniboxSuggestionUiType.TAIL_SUGGESTION,
                parent ->
                        new BaseSuggestionView<TailSuggestionView>(
                                new TailSuggestionView(parent.getContext())),
                new BaseSuggestionViewBinder<TailSuggestionView>(TailSuggestionViewBinder::bind));

        registerType(
                OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION,
                parent ->
                        new BaseSuggestionView<View>(
                                parent.getContext(), R.layout.omnibox_basic_suggestion),
                new BaseSuggestionViewBinder<View>(SuggestionViewViewBinder::bind));

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

        registerType(
                OmniboxSuggestionUiType.QUERY_TILES,
                BaseCarouselSuggestionItemViewBuilder::createView,
                BaseCarouselSuggestionViewBinder::bind);
    }

    /* package */ void recordSessionMetrics() {
        if (mNumSessionViewsBound > 0) {
            OmniboxMetrics.recordSuggestionViewReuseStats(
                    mNumSessionViewsCreated,
                    100
                            * (mNumSessionViewsBound - mNumSessionViewsCreated)
                            / mNumSessionViewsBound);
        }
        mNumSessionViewsCreated = 0;
        mNumSessionViewsBound = 0;
    }

    @Override
    public void onViewRecycled(ViewHolder holder) {
        super.onViewRecycled(holder);
        holder.itemView.setSelected(false);
    }

    @Override
    // extend this
    protected View createView(ViewGroup parent, int viewType) {
        // This skips measuring Adapter.CreateViewHolder, which is final, but it capture
        // the creation of a view holder.
        try (TraceEvent tracing =
                        TraceEvent.scoped("OmniboxSuggestionsList.CreateView", "type:" + viewType);
                TimingMetric metric = OmniboxMetrics.recordSuggestionViewCreateTime();
                TimingMetric metric2 = OmniboxMetrics.recordSuggestionViewCreateWallTime()) {
            return super.createView(parent, viewType);
        }
    }

    @Override
    public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        mNumSessionViewsCreated++;
        return super.onCreateViewHolder(parent, viewType);
    }

    @Override
    public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
        mNumSessionViewsBound++;
        super.onBindViewHolder(holder, position);
    }
}
