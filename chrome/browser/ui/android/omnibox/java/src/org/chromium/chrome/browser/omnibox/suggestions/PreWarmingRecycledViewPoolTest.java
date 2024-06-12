// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.Arrays;

/** Unit tests for {@link PreWarmingRecycledViewPool}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PreWarmingRecycledViewPoolTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock View mView;

    private Context mContext;
    private OmniboxSuggestionsDropdownAdapter mAdapter;
    private PreWarmingRecycledViewPool mPool;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mAdapter =
                Mockito.spy(
                        new OmniboxSuggestionsDropdownAdapter(new ModelList()) {
                            @Override
                            protected View createView(ViewGroup parent, int viewType) {
                                return mView;
                            }

                            @Override
                            public @NonNull ViewHolder onCreateViewHolder(
                                    @NonNull ViewGroup parent, int viewType) {
                                return new ViewHolder(mView, null);
                            }
                        });
        mPool = new PreWarmingRecycledViewPool(mAdapter, mContext);
    }

    private void ensureNoViewsCreated() {
        assertEquals(0, mPool.getRecycledViewCount(OmniboxSuggestionUiType.EDIT_URL_SUGGESTION));
        assertEquals(0, mPool.getRecycledViewCount(OmniboxSuggestionUiType.TILE_NAVSUGGEST));
        assertEquals(0, mPool.getRecycledViewCount(OmniboxSuggestionUiType.HEADER));
        assertEquals(0, mPool.getRecycledViewCount(OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION));
        assertEquals(0, mPool.getRecycledViewCount(OmniboxSuggestionUiType.DEFAULT));
        assertEquals(0, mPool.getRecycledViewCount(OmniboxSuggestionUiType.ENTITY_SUGGESTION));
        assertEquals(0, mPool.getRecycledViewCount(OmniboxSuggestionUiType.ENTITY_SUGGESTION));
    }

    private void ensureAllViewsCreated() {
        assertEquals(1, mPool.getRecycledViewCount(OmniboxSuggestionUiType.EDIT_URL_SUGGESTION));
        assertEquals(1, mPool.getRecycledViewCount(OmniboxSuggestionUiType.TILE_NAVSUGGEST));
        assertEquals(1, mPool.getRecycledViewCount(OmniboxSuggestionUiType.HEADER));
        assertEquals(1, mPool.getRecycledViewCount(OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION));
        assertEquals(15, mPool.getRecycledViewCount(OmniboxSuggestionUiType.DEFAULT));
        assertEquals(3, mPool.getRecycledViewCount(OmniboxSuggestionUiType.ENTITY_SUGGESTION));

        View expectedView = mView;
        // null out mView so that newly-created ViewHolders will be distinct from pre-warmed ones.
        mView = null;
        for (var uiType :
                Arrays.asList(
                        OmniboxSuggestionUiType.EDIT_URL_SUGGESTION,
                        OmniboxSuggestionUiType.TILE_NAVSUGGEST,
                        OmniboxSuggestionUiType.HEADER,
                        OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION,
                        OmniboxSuggestionUiType.DEFAULT,
                        OmniboxSuggestionUiType.ENTITY_SUGGESTION)) {
            ViewHolder viewHolder = mPool.getRecycledView(uiType);
            assertNotNull(viewHolder);
            assertEquals(expectedView, viewHolder.itemView);
        }
    }

    @DisableFeatures(OmniboxFeatureList.OMNIBOX_ASYNC_VIEW_INFLATION)
    @Test
    public void testCreateViews() {
        mPool.onNativeInitialized();

        ensureNoViewsCreated();

        // Run first, then cancel.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        mPool.stopCreatingViews();
        ensureAllViewsCreated();
    }

    @DisableFeatures(OmniboxFeatureList.OMNIBOX_ASYNC_VIEW_INFLATION)
    @Test
    public void stopCreatingViews_noViewsCreatedWhenCanceled() {
        mPool.onNativeInitialized();
        ensureNoViewsCreated();

        // Cancel, then run.
        mPool.stopCreatingViews();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        ensureNoViewsCreated();
    }

    @DisableFeatures(OmniboxFeatureList.OMNIBOX_ASYNC_VIEW_INFLATION)
    @Test
    public void destroy_cancelsViewCreation() {
        mPool.onNativeInitialized();
        ensureNoViewsCreated();

        // Destroy, then run.
        mPool.destroy();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        ensureNoViewsCreated();
    }

    @EnableFeatures(OmniboxFeatureList.OMNIBOX_ASYNC_VIEW_INFLATION)
    @Test
    public void createViews_noViewsCreatedOnLowEndDevices() {
        OmniboxFeatures.setIsLowMemoryDeviceForTesting(true);
        mPool.onNativeInitialized();
        ensureNoViewsCreated();
    }

    @EnableFeatures(OmniboxFeatureList.OMNIBOX_ASYNC_VIEW_INFLATION)
    @Test
    public void createViews_noViewsCreatedIfCanceledBeforeNative() {
        mPool.stopCreatingViews();
        mPool.onNativeInitialized();
        ensureNoViewsCreated();
    }

    @EnableFeatures(OmniboxFeatureList.OMNIBOX_ASYNC_VIEW_INFLATION)
    @Test
    public void createViews_recordViewCreated() {
        ensureNoViewsCreated();

        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Omnibox.SuggestionView.CreatedType",
                        OmniboxSuggestionUiType.DEFAULT)) {
            assertNull(mPool.getRecycledView(OmniboxSuggestionUiType.DEFAULT));
        }
    }

    @EnableFeatures(OmniboxFeatureList.OMNIBOX_ASYNC_VIEW_INFLATION)
    @Test
    public void createViews_recordViewReused() {
        mPool.onNativeInitialized();
        ensureAllViewsCreated();

        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Omnibox.SuggestionView.ReusedType",
                        OmniboxSuggestionUiType.DEFAULT)) {
            assertNotNull(mPool.getRecycledView(OmniboxSuggestionUiType.DEFAULT));
        }
    }

    @EnableFeatures(OmniboxFeatureList.OMNIBOX_ASYNC_VIEW_INFLATION)
    @Test
    public void createViews_viewsCreatedSynchronouslyWhenUsingAsyncViewInflater() {
        mPool.onNativeInitialized();
        ensureAllViewsCreated();
    }
}
