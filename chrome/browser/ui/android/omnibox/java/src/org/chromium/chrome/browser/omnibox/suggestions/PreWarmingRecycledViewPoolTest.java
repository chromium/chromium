// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertNotNull;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.Context;
import android.os.Handler;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.Arrays;

/**
 * Unit tests for {@link PreWarmingRecycledViewPool}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class PreWarmingRecycledViewPoolTest {
    public @Rule TestRule mProcessor = new Features.JUnitProcessor();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Handler mHandler;
    @Mock
    private View mView;

    private Context mContext;
    private OmniboxSuggestionsDropdownAdapter mAdapter;
    private PreWarmingRecycledViewPool mPool;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mAdapter = Mockito.spy(new OmniboxSuggestionsDropdownAdapter(new ModelList()) {
            @Override
            protected View createView(ViewGroup parent, int viewType) {
                return mView;
            }

            @Override
            @NonNull
            public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
                return new ViewHolder(mView, null);
            }
        });
        mPool = new PreWarmingRecycledViewPool(mAdapter, mContext, mHandler);
    }

    @EnableFeatures({ChromeFeatureList.OMNIBOX_WARM_RECYCLED_VIEW_POOL})
    @Test
    public void testCreateViews() {
        doAnswer((invocation -> {
            ((Runnable) invocation.getArgument(0)).run();
            return null;
        }))
                .when(mHandler)
                .postDelayed(any(Runnable.class), anyLong());
        mPool.onNativeInitialized();
        mPool.stopCreatingViews();

        assertEquals(1, mPool.getRecycledViewCount(OmniboxSuggestionUiType.EDIT_URL_SUGGESTION));
        assertEquals(1, mPool.getRecycledViewCount(OmniboxSuggestionUiType.TILE_NAVSUGGEST));
        assertEquals(1, mPool.getRecycledViewCount(OmniboxSuggestionUiType.HEADER));
        assertEquals(1, mPool.getRecycledViewCount(OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION));
        assertEquals(15, mPool.getRecycledViewCount(OmniboxSuggestionUiType.DEFAULT));
        assertEquals(3, mPool.getRecycledViewCount(OmniboxSuggestionUiType.ENTITY_SUGGESTION));

        View expectedView = mView;
        // null out mView so that newly-created ViewHolders will be distinct from pre-warmed ones.
        mView = null;
        for (var uiType : Arrays.asList(OmniboxSuggestionUiType.EDIT_URL_SUGGESTION,
                     OmniboxSuggestionUiType.TILE_NAVSUGGEST, OmniboxSuggestionUiType.HEADER,
                     OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION, OmniboxSuggestionUiType.DEFAULT,
                     OmniboxSuggestionUiType.ENTITY_SUGGESTION)) {
            ViewHolder viewHolder = mPool.getRecycledView(uiType);
            assertNotNull(viewHolder);
            assertEquals(expectedView, viewHolder.itemView);
        }
    }

    @DisableFeatures({ChromeFeatureList.OMNIBOX_WARM_RECYCLED_VIEW_POOL})
    @Test
    public void testCreateViews_featureDisabled() {
        doAnswer((invocation -> {
            ((Runnable) invocation.getArgument(0)).run();
            return null;
        }))
                .when(mHandler)
                .postDelayed(any(Runnable.class), anyLong());
        mPool.onNativeInitialized();

        verifyNoMoreInteractions(mAdapter);
        verifyNoMoreInteractions(mHandler);
    }

    @EnableFeatures({ChromeFeatureList.OMNIBOX_WARM_RECYCLED_VIEW_POOL})
    @Test
    public void testStopCreating() {
        mPool.onNativeInitialized();
        verify(mHandler, times(22)).postDelayed(any(Runnable.class), anyLong());
        mPool.getRecycledView(OmniboxSuggestionUiType.DEFAULT);
        verify(mHandler).removeCallbacks(null);

        mPool.getRecycledView(OmniboxSuggestionUiType.DEFAULT);
        verify(mHandler, times(1)).removeCallbacks(null);
    }
}
