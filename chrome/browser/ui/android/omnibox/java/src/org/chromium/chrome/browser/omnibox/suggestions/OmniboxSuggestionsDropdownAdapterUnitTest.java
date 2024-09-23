// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter.ViewHolder;

import java.util.Set;

/** Unit tests for the OmniboxSuggestionsDropdownAdapter component. */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxSuggestionsDropdownAdapterUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    private @Mock DropdownItemProcessor mProcessor;
    private Context mContext;
    private FrameLayout mContainer;
    private ModelList mModel;
    private OmniboxSuggestionsDropdownAdapter mAdapter;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mContainer = new FrameLayout(mContext);
        mModel = new ModelList();
        mAdapter = new OmniboxSuggestionsDropdownAdapter(mModel);
    }

    @Test
    public void createView_allUiTypesHaveAssociatedViewTypes() {
        Set<Integer> deprecatedViewTypes = Set.of();

        for (@OmniboxSuggestionUiType int type = OmniboxSuggestionUiType.DEFAULT;
                type < OmniboxSuggestionUiType.COUNT;
                type++) {
            if (deprecatedViewTypes.contains(type)) continue;

            var view = mAdapter.createView(mContainer, type);
            // Each view type should have a corresponding view object.
            // The only exception are the deprecated views - exceptions should be explicitly handled
            // here.
            assertNotNull(view);
            // View creation does not immediately mean view is retained.
            assertEquals(0, mAdapter.getItemCount());
        }
    }

    @Test
    public void onCreateViewHolder_retainsItemsByType() {
        for (@OmniboxSuggestionUiType int type = OmniboxSuggestionUiType.DEFAULT;
                type < OmniboxSuggestionUiType.COUNT;
                type++) {
            ViewHolder viewHolder = mAdapter.onCreateViewHolder(mContainer, type);
            assertNotNull(viewHolder);
            assertNotNull(viewHolder.itemView);
        }
    }

    @Test
    public void onViewRecycled_deselectAnyPreviouslySelectedViews() {
        // This test confirms that the recycled views do not carry Selected attribute when reused.
        var viewHolder = mAdapter.onCreateViewHolder(mContainer, OmniboxSuggestionUiType.DEFAULT);
        var view = viewHolder.itemView;
        view.setSelected(true);
        assertTrue(view.isSelected());
        mAdapter.onViewRecycled(viewHolder);
        assertFalse(view.isSelected());
    }

    @Test
    public void onBindViewHolder_allItemsMustSupportDropdownCommonProperties() {
        // These properties must be respected by all Dropdown items.
        var commonModel =
                new PropertyModel.Builder(DropdownCommonProperties.ALL_KEYS)
                        .with(DropdownCommonProperties.SHOW_DIVIDER, true)
                        .with(DropdownCommonProperties.BG_TOP_CORNER_ROUNDED, false)
                        .with(DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED, false)
                        .build();

        for (@OmniboxSuggestionUiType int type = OmniboxSuggestionUiType.DEFAULT;
                type < OmniboxSuggestionUiType.COUNT;
                type++) {
            doReturn(type).when(mProcessor).getViewTypeId();
            doReturn(commonModel).when(mProcessor).createModel();

            mModel.add(new DropdownItemViewInfo(mProcessor, commonModel, null));

            var viewHolder = mAdapter.onCreateViewHolder(mContainer, type);
            mAdapter.onBindViewHolder(viewHolder, 0);
            assertNotNull(viewHolder.model);

            // Confirm that view recycling decouples View from Model.
            mAdapter.onViewRecycled(viewHolder);
            assertNull(viewHolder.model);

            mModel.clear();
        }
    }

    @Test
    public void recordSuggestionViewReuseStats_noViewsReused() {
        // Create suggestion of every type, and bind it once.
        // These suggestions are not reused, so the reuse stats should be 0.
        for (@OmniboxSuggestionUiType int type = OmniboxSuggestionUiType.DEFAULT;
                type < OmniboxSuggestionUiType.COUNT;
                type++) {
            doReturn(type).when(mProcessor).getViewTypeId();
            mModel.add(new DropdownItemViewInfo(mProcessor, new PropertyModel(), null));

            var viewHolder = mAdapter.onCreateViewHolder(mContainer, type);
            mAdapter.onBindViewHolder(viewHolder, 0);
            mAdapter.onViewRecycled(viewHolder);

            mModel.clear();
        }

        // Confirm all views created, but none of them have been reused (reuse at 0%).
        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Android.Omnibox.SuggestionView.SessionViewsCreated", 10)
                        .expectIntRecords("Android.Omnibox.SuggestionView.SessionViewsReused", 0)
                        .build()) {
            mAdapter.recordSessionMetrics();
        }
    }

    @Test
    public void recordSuggestionViewReuseStats_allViewsReused() {
        // Create suggestion of every type, and bind it twice.
        // 100% reuse is a theoretical limit, indicating that all created views have been reused
        // infinite amount of times.
        for (@OmniboxSuggestionUiType int type = OmniboxSuggestionUiType.DEFAULT;
                type < OmniboxSuggestionUiType.COUNT;
                type++) {
            doReturn(type).when(mProcessor).getViewTypeId();
            mModel.add(new DropdownItemViewInfo(mProcessor, new PropertyModel(), null));

            var viewHolder = mAdapter.onCreateViewHolder(mContainer, type);
            mAdapter.onBindViewHolder(viewHolder, 0);
            mAdapter.onViewRecycled(viewHolder);

            // Reuse view.
            mAdapter.onBindViewHolder(viewHolder, 0);
            mAdapter.onViewRecycled(viewHolder);

            mModel.clear();
        }

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Android.Omnibox.SuggestionView.SessionViewsCreated", 10)
                        .expectIntRecords("Android.Omnibox.SuggestionView.SessionViewsReused", 50)
                        .build()) {
            mAdapter.recordSessionMetrics();
        }
    }

    @Test
    public void recordSuggestionViewReuseStats_noViewsBound() {
        // When views have been created but never bound - do not capture any metrics.
        for (@OmniboxSuggestionUiType int type = OmniboxSuggestionUiType.DEFAULT;
                type < OmniboxSuggestionUiType.COUNT;
                type++) {
            doReturn(type).when(mProcessor).getViewTypeId();
            mModel.add(new DropdownItemViewInfo(mProcessor, new PropertyModel(), null));
            mAdapter.onCreateViewHolder(mContainer, type);
            mModel.clear();
        }

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.Omnibox.SuggestionView.SessionViewsCreated")
                        .expectNoRecords("Android.Omnibox.SuggestionView.SessionViewsReused")
                        .build()) {
            mAdapter.recordSessionMetrics();
        }
    }
}
