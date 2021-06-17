// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.res.Resources;
import android.view.View;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.chrome.R;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for {@link BaseCarouselSuggestionViewBinder}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BaseCarouselSuggestionViewBinderUnitTest {
    static final int SUGGESTION_VERTICAL_PADDING = 123;

    @Mock
    BaseCarouselSuggestionView mView;

    @Mock
    TextView mHeaderTextView;

    @Mock
    View mHeaderView;

    @Mock
    View mItemView;

    @Mock
    SimpleRecyclerViewAdapter mAdapter;

    @Mock
    Resources mResources;

    ModelList mTiles;

    PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel = new PropertyModel(BaseCarouselSuggestionViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mView, BaseCarouselSuggestionViewBinder::bind);

        mTiles = new ModelList();

        when(mView.getHeaderTextView()).thenReturn(mHeaderTextView);
        when(mView.getHeaderView()).thenReturn(mHeaderView);
        when(mView.getAdapter()).thenReturn(mAdapter);
        when(mAdapter.getModelList()).thenReturn(mTiles);
        when(mView.getResources()).thenReturn(mResources);

        when(mResources.getDimensionPixelSize(eq(R.dimen.omnibox_carousel_suggestion_padding)))
                .thenReturn(SUGGESTION_VERTICAL_PADDING);
    }

    @Test
    public void headerTitle_set() {
        mModel.set(BaseCarouselSuggestionViewProperties.TITLE, "title");
        verify(mHeaderTextView, times(1)).setText(eq("title"));
        verifyNoMoreInteractions(mHeaderTextView);
    }

    @Test
    public void headerTitle_updateToSameIsNoOp() {
        mModel.set(BaseCarouselSuggestionViewProperties.TITLE, "title");
        reset(mHeaderTextView);
        mModel.set(BaseCarouselSuggestionViewProperties.TITLE, "title");
        verifyNoMoreInteractions(mHeaderTextView);
    }

    @Test
    public void modelList_setItems() {
        final List<ListItem> tiles = new ArrayList<>();
        tiles.add(new ListItem(0, null));
        tiles.add(new ListItem(0, null));
        tiles.add(new ListItem(0, null));

        Assert.assertEquals(0, mTiles.size());
        mModel.set(BaseCarouselSuggestionViewProperties.TILES, tiles);
        Assert.assertEquals(3, mTiles.size());
        Assert.assertEquals(tiles.get(0), mTiles.get(0));
        Assert.assertEquals(tiles.get(1), mTiles.get(1));
        Assert.assertEquals(tiles.get(2), mTiles.get(2));
    }

    @Test
    public void modelList_clearItems() {
        final List<ListItem> tiles = new ArrayList<>();
        tiles.add(new ListItem(0, null));
        tiles.add(new ListItem(0, null));
        tiles.add(new ListItem(0, null));

        Assert.assertEquals(0, mTiles.size());
        mModel.set(BaseCarouselSuggestionViewProperties.TILES, tiles);
        Assert.assertEquals(3, mTiles.size());
        mModel.set(BaseCarouselSuggestionViewProperties.TILES, null);
        Assert.assertEquals(0, mTiles.size());
    }

    @Test
    public void headerTitle_visibilityChangeAltersTopPadding() {
        mModel.set(BaseCarouselSuggestionViewProperties.SHOW_TITLE, true);
        verify(mHeaderView, times(1)).setVisibility(eq(View.VISIBLE));
        verify(mHeaderView, times(1)).setVisibility(anyInt());
        verify(mView, times(1))
                .setPaddingRelative(eq(0), eq(0), eq(0), eq(SUGGESTION_VERTICAL_PADDING));
        verify(mView, times(1)).setPaddingRelative(anyInt(), anyInt(), anyInt(), anyInt());

        mModel.set(BaseCarouselSuggestionViewProperties.SHOW_TITLE, false);
        verify(mHeaderView, times(1)).setVisibility(eq(View.GONE));
        verify(mHeaderView, times(2)).setVisibility(anyInt());
        verify(mView, times(1))
                .setPaddingRelative(eq(0), eq(SUGGESTION_VERTICAL_PADDING), eq(0),
                        eq(SUGGESTION_VERTICAL_PADDING));
        verify(mView, times(2)).setPaddingRelative(anyInt(), anyInt(), anyInt(), anyInt());
    }
}
