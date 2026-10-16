// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView.RecycledViewPool;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;

/**
 * RecyclerView pool that:
 *
 * <ul>
 *   <li>Pre-creates a hardcoded set of ViewHolders.
 *   <li>Records the performance of the view recycling mechanism.
 * </ul>
 */
@NullMarked
public class PreWarmingRecycledViewPool extends RecycledViewPool {
    @VisibleForTesting static final int PRE_WARMED_EDIT_URL_SUGGESTION_VIEW_COUNT = 1;
    @VisibleForTesting static final int PRE_WARMED_TILE_NAVSUGGEST_VIEW_COUNT = 1;
    @VisibleForTesting static final int PRE_WARMED_CLIPBOARD_SUGGESTION_VIEW_COUNT = 1;
    @VisibleForTesting static final int PRE_WARMED_DEFAULT_VIEW_COUNT = 15;
    @VisibleForTesting static final int PRE_WARMED_ENTITY_SUGGESTION_VIEW_COUNT = 3;

    PreWarmingRecycledViewPool(OmniboxViewHolderFactory factory, Context context) {
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
        setMaxRecycledViews(OmniboxSuggestionUiType.TILE_NAVSUGGEST, 1);

        preWarmViews(factory, context);
    }

    /**
     * Immediately creates all pre-warmed views and places them into the pool.
     *
     * @param factory The factory used to create the ViewHolders.
     * @param context The context used to create the placeholder parent view.
     */
    private void preWarmViews(OmniboxViewHolderFactory factory, Context context) {
        try (TraceEvent t = TraceEvent.scoped("PreWarmingRecycledViewPool.preWarmViews")) {
            if (!OmniboxCapabilities.shouldPreWarmRecyclerViewPool()) return;
            OmniboxMetrics.recordPreWarmingThreadMatchesExpectedThread(
                    !ThreadUtils.runningOnUiThread());

            TimeUtils.UptimeMillisTimer wallTimer = new TimeUtils.UptimeMillisTimer();
            TimeUtils.CurrentThreadTimeMillisTimer threadTimer =
                    new TimeUtils.CurrentThreadTimeMillisTimer();

            FrameLayout parent = new FrameLayout(context);
            preWarmViewType(
                    factory,
                    parent,
                    OmniboxSuggestionUiType.EDIT_URL_SUGGESTION,
                    PRE_WARMED_EDIT_URL_SUGGESTION_VIEW_COUNT);
            preWarmViewType(
                    factory,
                    parent,
                    OmniboxSuggestionUiType.TILE_NAVSUGGEST,
                    PRE_WARMED_TILE_NAVSUGGEST_VIEW_COUNT);
            preWarmViewType(
                    factory,
                    parent,
                    OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION,
                    PRE_WARMED_CLIPBOARD_SUGGESTION_VIEW_COUNT);
            preWarmViewType(
                    factory,
                    parent,
                    OmniboxSuggestionUiType.DEFAULT,
                    PRE_WARMED_DEFAULT_VIEW_COUNT);
            preWarmViewType(
                    factory,
                    parent,
                    OmniboxSuggestionUiType.ENTITY_SUGGESTION,
                    PRE_WARMED_ENTITY_SUGGESTION_VIEW_COUNT);

            OmniboxMetrics.recordPreWarmingViewsThreadTime(threadTimer.getElapsedMillis());
            OmniboxMetrics.recordPreWarmingViewsWallTime(wallTimer.getElapsedMillis());
        }
    }

    /**
     * Creates ViewHolders of the given type and places them into the pool.
     *
     * @param factory The factory used to create the ViewHolders.
     * @param parent The placeholder parent passed to the factory.
     * @param viewType The type of the ViewHolders to create.
     * @param count The number of ViewHolders to create.
     */
    private void preWarmViewType(
            OmniboxViewHolderFactory factory,
            FrameLayout parent,
            @OmniboxSuggestionUiType int viewType,
            int count) {
        for (int index = 0; index < count; ++index) {
            putRecycledView(factory.createViewHolderForPool(parent, viewType));
        }
    }

    @Override
    public @Nullable ViewHolder getRecycledView(int viewType) {
        ViewHolder result = super.getRecycledView(viewType);
        if (result == null) {
            OmniboxMetrics.recordSuggestionsViewCreatedType(viewType);
        } else {
            OmniboxMetrics.recordSuggestionsViewReusedType(viewType);
        }
        return result;
    }
}
