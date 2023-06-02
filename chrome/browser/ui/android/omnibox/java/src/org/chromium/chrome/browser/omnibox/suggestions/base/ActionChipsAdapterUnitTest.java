// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link ActionChipsAdapter}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActionChipsAdapterUnitTest {
    private static final int CHIP_1_INDEX = 0;
    private static final int CHIP_2_INDEX = 1;
    private static final int CHIP_3_WITH_NO_VIEW_INDEX = 2;
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock View mHeader;
    private @Mock View mChip1;
    private @Mock View mChip2;
    private @Mock LayoutManager mLayoutManager;

    private ModelList mModel = new ModelList();
    private RecyclerView mRecyclerView;
    private ActionChipsAdapter mAdapter = new ActionChipsAdapter(mModel);

    @Before
    public void setUp() {
        // Chip with View, 0
        mModel.add(new ListItem(
                ActionChipsProperties.ViewType.CHIP, new PropertyModel(ChipProperties.ALL_KEYS)));
        doReturn(mChip1).when(mLayoutManager).findViewByPosition(CHIP_1_INDEX);

        // Chip with View, 1
        mModel.add(new ListItem(
                ActionChipsProperties.ViewType.CHIP, new PropertyModel(ChipProperties.ALL_KEYS)));
        doReturn(mChip2).when(mLayoutManager).findViewByPosition(CHIP_2_INDEX);

        // Chip with no View, 2
        mModel.add(new ListItem(
                ActionChipsProperties.ViewType.CHIP, new PropertyModel(ChipProperties.ALL_KEYS)));

        mRecyclerView = new RecyclerView(ContextUtils.getApplicationContext());
        mRecyclerView.setLayoutManager(mLayoutManager);
        mRecyclerView.setAdapter(mAdapter);
    }

    @Test
    public void setSelectedItem_fromNoSelection() {
        mAdapter.setSelectedItem(CHIP_1_INDEX);
        verify(mChip1).setSelected(true);
        verifyNoMoreInteractions(mHeader, mChip1, mChip2);
    }

    @Test
    public void setSelectedItem_intoNewSelection() {
        mAdapter.setSelectedItem(CHIP_1_INDEX);
        verify(mChip1).setSelected(true);

        mAdapter.setSelectedItem(CHIP_2_INDEX);
        verify(mChip1).setSelected(false);
        verify(mChip2).setSelected(true);
        verifyNoMoreInteractions(mHeader, mChip1, mChip2);
    }

    @Test
    public void setSelectedItem_intoNoView() {
        mAdapter.setSelectedItem(CHIP_3_WITH_NO_VIEW_INDEX);
        verifyNoMoreInteractions(mHeader, mChip1, mChip2);
    }

    @Test
    public void setSelectedItem_fromNoView() {
        mAdapter.setSelectedItem(CHIP_3_WITH_NO_VIEW_INDEX);
        verifyNoMoreInteractions(mHeader, mChip1, mChip2);

        mAdapter.setSelectedItem(CHIP_2_INDEX);
        verify(mChip2).setSelected(true);
        verifyNoMoreInteractions(mHeader, mChip1, mChip2);
    }

    @Test
    public void setSelectedItem_intoNoSelection() {
        mAdapter.setSelectedItem(CHIP_1_INDEX);
        verify(mChip1).setSelected(true);

        mAdapter.setSelectedItem(RecyclerView.NO_POSITION);
        verify(mChip1).setSelected(false);
        verifyNoMoreInteractions(mHeader, mChip1, mChip2);
    }

    @Test(expected = AssertionError.class)
    public void setSelectedItem_rejectPositionOutOfRange() {
        mAdapter.setSelectedItem(mAdapter.getItemCount());
    }

    // The following tests build on top of the setSelectedItem tests.
    // The method setSelectedItem stays at the heart of all of the functionality below
    // and is tested thoroughly by dedicated tests.
    @Test
    public void selectNextItem_loopsAround() {
        assertEquals(RecyclerView.NO_POSITION, mAdapter.getSelectedItemForTesting());

        mAdapter.selectNextItem();
        assertEquals(CHIP_1_INDEX, mAdapter.getSelectedItemForTesting());
        verify(mChip1).setSelected(true);
        verify(mLayoutManager).scrollToPosition(CHIP_1_INDEX);

        mAdapter.selectNextItem();
        assertEquals(CHIP_2_INDEX, mAdapter.getSelectedItemForTesting());
        verify(mChip1).setSelected(false);
        verify(mChip2).setSelected(true);
        verify(mLayoutManager).scrollToPosition(CHIP_2_INDEX);

        mAdapter.selectNextItem();
        assertEquals(CHIP_3_WITH_NO_VIEW_INDEX, mAdapter.getSelectedItemForTesting());
        verify(mChip2).setSelected(false);
        verify(mLayoutManager).scrollToPosition(CHIP_3_WITH_NO_VIEW_INDEX);

        mAdapter.selectNextItem();
        assertEquals(RecyclerView.NO_POSITION, mAdapter.getSelectedItemForTesting());

        verifyNoMoreInteractions(mHeader, mChip1, mChip2);
    }

    @Test
    public void selectPreviousItem_loopsAround() {
        assertEquals(RecyclerView.NO_POSITION, mAdapter.getSelectedItemForTesting());

        mAdapter.selectPreviousItem();
        assertEquals(CHIP_3_WITH_NO_VIEW_INDEX, mAdapter.getSelectedItemForTesting());
        verify(mLayoutManager).scrollToPosition(CHIP_3_WITH_NO_VIEW_INDEX);

        mAdapter.selectPreviousItem();
        assertEquals(CHIP_2_INDEX, mAdapter.getSelectedItemForTesting());
        verify(mChip2).setSelected(true);
        verify(mLayoutManager).scrollToPosition(CHIP_2_INDEX);

        mAdapter.selectPreviousItem();
        assertEquals(CHIP_1_INDEX, mAdapter.getSelectedItemForTesting());
        verify(mChip2).setSelected(false);
        verify(mChip1).setSelected(true);
        verify(mLayoutManager).scrollToPosition(CHIP_1_INDEX);

        mAdapter.selectPreviousItem();
        assertEquals(RecyclerView.NO_POSITION, mAdapter.getSelectedItemForTesting());
        verify(mChip1).setSelected(false);

        verifyNoMoreInteractions(mHeader, mChip1, mChip2);
    }

    @Test
    public void resetSelection_fromNoSelection() {
        mAdapter.setSelectedItem(CHIP_1_INDEX);
        verify(mChip1).setSelected(true);
        verifyNoMoreInteractions(mHeader, mChip1, mChip2);

        mAdapter.resetSelection();
        verify(mChip1).setSelected(false);

        assertEquals(RecyclerView.NO_POSITION, mAdapter.getSelectedItemForTesting());
        verifyNoMoreInteractions(mHeader, mChip1, mChip2);
    }
}
