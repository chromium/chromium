// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** ModelListAdapter for OmniboxSuggestionsDropdown (RecyclerView version). */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public class OmniboxSuggestionsDropdownAdapter extends SimpleRecyclerViewAdapter {
    private int mSelectedItem = RecyclerView.NO_POSITION;
    private LayoutManager mLayoutManager;
    private int mNumSessionViewsCreated;
    private int mNumSessionViewsBound;

    OmniboxSuggestionsDropdownAdapter(ModelList data) {
        super(data);
    }

    @Override
    public void onAttachedToRecyclerView(@NonNull RecyclerView view) {
        super.onAttachedToRecyclerView(view);
        mLayoutManager = view.getLayoutManager();
        mSelectedItem = RecyclerView.NO_POSITION;
        if (OmniboxFeatures.shouldShowSmallestMargins(view.getContext())) {
            view.addItemDecoration(new SuggestionHorizontalDivider(view.getContext()));
        }
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
        if (holder == null || holder.itemView == null) return;
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
