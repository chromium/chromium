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

import org.chromium.base.TimeUtils;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;

import java.util.ArrayList;
import java.util.List;

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

    private final OmniboxViewHolderFactory mViewHolderFactory;
    private final @Nullable Handler mHandler;
    private final FrameLayout mPlaceholderParent;
    private final Thread mThread = Thread.currentThread();
    private boolean mStopCreatingViews;
    private final List<ViewHolder> mPrewarmedViews = new ArrayList<>(22);
    private long mCumulativePrewarmWallTimeMs;
    private long mCumulativePrewarmThreadTimeMs;
    private int mExpectedViewCount;

    PreWarmingRecycledViewPool(OmniboxViewHolderFactory factory, Context context) {
        mViewHolderFactory = factory;
        mHandler =
                OmniboxFeatures.sAsyncViewInflation.isEnabled()
                        // If AsyncViewInflation is enabled, we use AsyncViewStub to handle
                        // asynchrony and we don't need to do it ourselves.
                        ? null
                        // Otherwise, we handle asynchrony.
                        : new Handler();
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

        if (OmniboxFeatures.sAsyncViewInflation.isEnabled()) {
            startCreatingViews();
        }
    }

    public void destroy() {
        stopCreatingViews();
        clear();
    }

    public void onNativeInitialized() {
        if (!OmniboxFeatures.sAsyncViewInflation.isEnabled()) {
            startCreatingViews();
        }
    }

    /**
     * Starts creating views. If mHandler is not null (async view inflation disabled), this will
     * immediately post a separate delayed task for every view we intend to create with a delay
     * equal to STEP_MILLIS. If mHandler is null (async view inflation enabled), this will
     * immediately create all views.
     */
    public void startCreatingViews() {
        assert mThread == Thread.currentThread()
                : "startCreatingViews must be called on the same thread the pool was created on";
        try (TraceEvent t = TraceEvent.scoped("PreWarmingRecycledViewPool.startCreatingViews")) {
            if (mStopCreatingViews || !OmniboxCapabilities.shouldPreWarmRecyclerViewPool()) return;
            for (var viewTypeAndCount : mViewsToCreate) {
                mExpectedViewCount += viewTypeAndCount.count;
                for (int index = 0; index < viewTypeAndCount.count; ++index) {
                    if (mHandler != null) {
                        Runnable createViewRunnable =
                                () -> createViewHolder(viewTypeAndCount.viewType);
                        final long delay = STEP_MILLIS * (index + 1);
                        mHandler.postDelayed(createViewRunnable, delay);
                    } else {
                        createViewHolder(viewTypeAndCount.viewType);
                    }
                }
            }

            // Synchronously apply all views.
            if (mHandler == null) {
                stopCreatingViews();
            }
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
        if (mHandler != null) {
            mHandler.removeCallbacksAndMessages(null);
            OmniboxMetrics.recordPreWarmingViewsThreadTime(mCumulativePrewarmThreadTimeMs);
            OmniboxMetrics.recordPreWarmingViewsWallTime(mCumulativePrewarmWallTimeMs);
            OmniboxMetrics.recordPreWarmedViewsCount(mPrewarmedViews.size());
        }

        putViewsIntoPool();
    }

    private void createViewHolder(@OmniboxSuggestionUiType int viewType) {
        if (mStopCreatingViews) return;
        TimeUtils.UptimeMillisTimer wallTimer = new TimeUtils.UptimeMillisTimer();
        TimeUtils.CurrentThreadTimeMillisTimer threadTimer =
                new TimeUtils.CurrentThreadTimeMillisTimer();

        try (TraceEvent t = TraceEvent.scoped("PreWarmingRecycledViewPool.createNextViewHolder")) {
            mPrewarmedViews.add(
                    mViewHolderFactory.createViewHolderForPool(mPlaceholderParent, viewType));
        }

        if (!OmniboxFeatures.sAsyncViewInflation.isEnabled()) {
            mCumulativePrewarmWallTimeMs += wallTimer.getElapsedMillis();
            mCumulativePrewarmThreadTimeMs += threadTimer.getElapsedMillis();
            if (mPrewarmedViews.size() == mExpectedViewCount) {
                stopCreatingViews();
            }
        }
    }

    private void putViewsIntoPool() {
        for (var viewHolder : mPrewarmedViews) {
            putRecycledView(viewHolder);
        }
        mPrewarmedViews.clear();
    }

    @Override
    public @Nullable ViewHolder getRecycledView(int viewType) {
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
