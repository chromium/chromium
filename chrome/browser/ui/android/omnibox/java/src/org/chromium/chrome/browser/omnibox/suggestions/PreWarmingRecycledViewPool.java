// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.os.Handler;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView.RecycledViewPool;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

/**
 * RecyclerView pool that:
 *
 * <ul>
 *   <li>Pre-creates a hardcoded set of ViewHolders.
 *   <li>Records the performance of the view recycling mechanism.
 * </ul>
 */
public class PreWarmingRecycledViewPool extends RecycledViewPool {
    private static final long STEP_MILLIS = 50;

    private static class ViewTypeAndCount {
        public final int viewType;
        public final int count;

        public ViewTypeAndCount(int viewType, int count) {
            this.viewType = viewType;
            assert count > 0;
            this.count = count;
        }
    }

    private final ViewTypeAndCount[] mViewsToCreate =
            new ViewTypeAndCount[] {
                new ViewTypeAndCount(OmniboxSuggestionUiType.EDIT_URL_SUGGESTION, 1),
                new ViewTypeAndCount(OmniboxSuggestionUiType.TILE_NAVSUGGEST, 1),
                new ViewTypeAndCount(OmniboxSuggestionUiType.HEADER, 1),
                new ViewTypeAndCount(OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION, 1),
                new ViewTypeAndCount(OmniboxSuggestionUiType.DEFAULT, 15),
                new ViewTypeAndCount(OmniboxSuggestionUiType.ENTITY_SUGGESTION, 3)
            };

    private OmniboxSuggestionsDropdownAdapter mAdapter;
    private final Optional<Handler> mHandler;
    private final FrameLayout mPlaceholderParent;
    private boolean mStopCreatingViews;
    private final List<ViewHolder> mPrewarmedViews = new ArrayList<>(22);

    PreWarmingRecycledViewPool(OmniboxSuggestionsDropdownAdapter adapter, Context context) {
        mAdapter = adapter;
        mHandler =
                OmniboxFeatures.sAsyncViewInflation.isEnabled()
                        // If AsyncViewInflation is enabled, we use AsyncViewStub to handle
                        // asynchrony and we
                        // don't need to do it ourselves.
                        ? Optional.empty()
                        // Otherwise, we handle asynchrony.
                        : Optional.of(new Handler());
        mPlaceholderParent = new FrameLayout(context);
        // The list below should include suggestions defined in OmniboxSuggestionUiType
        // and specify the maximum anticipated volume of suggestions of each type.
        // For readability reasons, keep the order of this list same as the order of
        // the types defined in OmniboxSuggestionUiType.
        setMaxRecycledViews(OmniboxSuggestionUiType.DEFAULT, 20);
        setMaxRecycledViews(OmniboxSuggestionUiType.EDIT_URL_SUGGESTION, 1);
        setMaxRecycledViews(OmniboxSuggestionUiType.ANSWER_SUGGESTION, 1);
        setMaxRecycledViews(OmniboxSuggestionUiType.ENTITY_SUGGESTION, 8);

        setMaxRecycledViews(OmniboxSuggestionUiType.TAIL_SUGGESTION, 15);
        setMaxRecycledViews(OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION, 1);
        setMaxRecycledViews(OmniboxSuggestionUiType.HEADER, 4);
        setMaxRecycledViews(OmniboxSuggestionUiType.TILE_NAVSUGGEST, 1);
        setMaxRecycledViews(OmniboxSuggestionUiType.GROUP_SEPARATOR, 1);
        setMaxRecycledViews(OmniboxSuggestionUiType.QUERY_TILES, 1);
    }

    public void destroy() {
        stopCreatingViews();
        mAdapter = null;
    }

    public void onNativeInitialized() {
        if (OmniboxFeatures.shouldPreWarmRecyclerViewPool()) {
            startCreatingViews();
        }
    }

    /**
     * Starts creating views. This will immediately post a separate delayed task for every view we
     * intend to create with a delay equal to STEP_MILLIS * order_of_view_creation.
     */
    public void startCreatingViews() {
        if (mStopCreatingViews) return;
        for (var viewTypeAndCount : mViewsToCreate) {
            for (int index = 0; index < viewTypeAndCount.count; ++index) {
                Runnable createViewRunnable = () -> createViewHolder(viewTypeAndCount.viewType);
                final long delay = STEP_MILLIS * (index + 1);
                mHandler.ifPresentOrElse(
                        h -> h.postDelayed(createViewRunnable, delay), createViewRunnable);
            }
        }

        // Synchronously apply all views.
        if (mHandler.isEmpty()) {
            putViewsIntoPool();
        }
    }

    /**
     * Stops the task posted by {@link #startCreatingViews()} from creating any more views and
     * removes it from the handler. Places any pre-created views into the pool.
     */
    @VisibleForTesting
    void stopCreatingViews() {
        if (mStopCreatingViews) return;
        mStopCreatingViews = true;
        mHandler.ifPresent(h -> h.removeCallbacks(null));
        putViewsIntoPool();
    }

    private void createViewHolder(@OmniboxSuggestionUiType int viewType) {
        if (mAdapter == null || mStopCreatingViews) return;
        try (TraceEvent t = TraceEvent.scoped("PreWarmingRecycledViewPool.createNextViewHolder")) {
            mPrewarmedViews.add(mAdapter.createViewHolder(mPlaceholderParent, viewType));
        }
    }

    private void putViewsIntoPool() {
        for (var viewHolder : mPrewarmedViews) {
            putRecycledView(viewHolder);
        }
        mPrewarmedViews.clear();
    }

    @Override
    public ViewHolder getRecycledView(int viewType) {
        stopCreatingViews();
        ViewHolder result = super.getRecycledView(viewType);
        if (result == null) {
            OmniboxMetrics.recordSuggestionsViewCreatedType(viewType);
        } else {
            OmniboxMetrics.recordSuggestionsViewReusedType(viewType);
        }
        return result;
    }
}
