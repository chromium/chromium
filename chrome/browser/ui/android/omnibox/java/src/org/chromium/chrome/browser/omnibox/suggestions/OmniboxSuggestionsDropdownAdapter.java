// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.TimingMetric;
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
    }

    /* package */ void recordSessionMetrics() {
        if (mNumSessionViewsBound > 0) {
            SuggestionsMetrics.recordSuggestionViewReuseStats(mNumSessionViewsCreated,
                    100 * (mNumSessionViewsBound - mNumSessionViewsCreated)
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

    /**
     * @return Index of the currently highlighted view.
     */
    int getSelectedViewIndex() {
        return mSelectedItem;
    }

    /** @return Currently selected view. */
    @Nullable
    View getSelectedView() {
        if (mLayoutManager == null) return null;
        if (mSelectedItem < 0 || mSelectedItem >= getItemCount()) return null;

        View currentSelectedView = mLayoutManager.findViewByPosition(mSelectedItem);
        if (currentSelectedView != null) {
            return currentSelectedView;
        }

        mSelectedItem = RecyclerView.NO_POSITION;
        return null;
    }

    /** Ensures selection is reset to beginning of the list. */
    void resetSelection() {
        setSelectedViewIndex(RecyclerView.NO_POSITION);
    }

    /**
     * Move focus to another view.
     *
     * @param index end index.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public boolean setSelectedViewIndex(int index) {
        if (mLayoutManager == null) return false;
        if (index != RecyclerView.NO_POSITION && (index < 0 || index >= getItemCount())) {
            return false;
        }

        View previousSelectedView = mLayoutManager.findViewByPosition(mSelectedItem);
        if (previousSelectedView != null) {
            previousSelectedView.setSelected(false);
        }

        mSelectedItem = index;
        mLayoutManager.scrollToPosition(index);

        View currentSelectedView = mLayoutManager.findViewByPosition(index);
        if (currentSelectedView != null) {
            currentSelectedView.setSelected(true);
        }

        return true;
    }

    @Override
    // extend this
    protected View createView(ViewGroup parent, int viewType) {
        // This skips measuring Adapter.CreateViewHolder, which is final, but it capture
        // the creation of a view holder.
        try (TraceEvent tracing =
                        TraceEvent.scoped("OmniboxSuggestionsList.CreateView", "type:" + viewType);
                TimingMetric metric = SuggestionsMetrics.recordSuggestionViewCreateTime()) {
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
