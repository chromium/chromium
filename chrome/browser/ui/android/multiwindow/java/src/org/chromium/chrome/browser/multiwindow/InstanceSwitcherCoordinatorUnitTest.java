// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout.LayoutParams;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import com.google.android.material.tabs.TabLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class InstanceSwitcherCoordinatorUnitTest {
    /** A LayoutManager that reports a fixed vertical scroll range. */
    private static final class FixedScrollRangeLayoutManager extends RecyclerView.LayoutManager {
        private int mVerticalScrollRange;

        @Override
        public RecyclerView.LayoutParams generateDefaultLayoutParams() {
            return new RecyclerView.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        @Override
        public boolean canScrollVertically() {
            return true;
        }

        @Override
        public int computeVerticalScrollRange(RecyclerView.State state) {
            return mVerticalScrollRange;
        }
    }

    private static final int MIN_COMMAND_ITEM_HEIGHT_PX = 64;
    private static final int ITEM_PADDING_HEIGHT_PX = 2;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private RecyclerView.Adapter mActiveListAdapter;
    @Mock private RecyclerView.Adapter mInactiveListAdapter;

    private View mDialogView;
    private TabLayout mTabHeaderRow;
    private FrameLayout mInstanceListContainer;
    private RecyclerView mActiveInstancesList;
    private RecyclerView mInactiveInstancesList;
    private View mCommandItem;
    private FixedScrollRangeLayoutManager mActiveListLayoutManager;

    @Before
    public void setup() {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        mDialogView = new View(context);
        mDialogView.setPadding(0, /* top= */ 16, 0, 0);

        mTabHeaderRow = new TabLayout(context);
        measureExactly(mTabHeaderRow, /* height= */ 50);

        mInstanceListContainer = new FrameLayout(context);

        mCommandItem = new View(context);
        measureExactly(mCommandItem, MIN_COMMAND_ITEM_HEIGHT_PX);

        mActiveListLayoutManager = new FixedScrollRangeLayoutManager();
        mActiveInstancesList = new RecyclerView(context);
        mActiveInstancesList.setLayoutManager(mActiveListLayoutManager);
        mActiveInstancesList.setAdapter(mActiveListAdapter);

        mInactiveInstancesList = new RecyclerView(context);
        mInactiveInstancesList.setAdapter(mInactiveListAdapter);
    }

    private static void measureExactly(View view, int height) {
        view.measure(
                MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));
    }

    private void setUpActiveList(int measuredHeight, int verticalScrollRange) {
        mActiveListLayoutManager.mVerticalScrollRange = verticalScrollRange;
        measureExactly(mActiveInstancesList, measuredHeight);
    }

    /** Simulates the XML spec. */
    private LayoutParams setInitialContainerLayoutParams() {
        var initialLayoutParams = new LayoutParams(LayoutParams.MATCH_PARENT, 0);
        initialLayoutParams.weight = 1;
        mInstanceListContainer.setLayoutParams(initialLayoutParams);
        return initialLayoutParams;
    }

    @Test
    public void testInstanceListGlobalLayoutListener_NoOpWhenDefault() {
        setInitialContainerLayoutParams();
        // Clear the pending layout request so that a later setLayoutParams() is observable.
        measureExactly(mInstanceListContainer, /* height= */ 100);
        mInstanceListContainer.layout(0, 0, 100, 100);
        assertFalse(mInstanceListContainer.isLayoutRequested());

        // Simulate a scrollable active instances list.
        setUpActiveList(/* measuredHeight= */ 200, /* verticalScrollRange= */ 250);

        // Run the GlobalLayoutListener callback.
        var listener =
                InstanceSwitcherCoordinator.addLayoutListeners(
                        mDialogView,
                        mTabHeaderRow,
                        mInstanceListContainer,
                        mActiveInstancesList,
                        mInactiveInstancesList,
                        /* isInactiveListShowingSupplier= */ () -> false,
                        mCommandItem,
                        MIN_COMMAND_ITEM_HEIGHT_PX,
                        ITEM_PADDING_HEIGHT_PX,
                        /* registerResizeListener= */ false);
        listener.onGlobalLayout();

        // Verify there is no update to layout params, since the layout should use the default spec.
        assertFalse(mInstanceListContainer.isLayoutRequested());
        LayoutParams params = (LayoutParams) mInstanceListContainer.getLayoutParams();
        assertEquals("Height is incorrect.", 0, params.height);
        assertEquals("Weight is incorrect.", 1, params.weight, 0);
    }

    @Test
    public void testInstanceListGlobalLayoutListener_NonScrollableList() {
        setInitialContainerLayoutParams();

        // Simulate a non-scrollable active instances list.
        setUpActiveList(/* measuredHeight= */ 200, /* verticalScrollRange= */ 200);

        // Run the GlobalLayoutListener callback.
        var listener =
                InstanceSwitcherCoordinator.addLayoutListeners(
                        mDialogView,
                        mTabHeaderRow,
                        mInstanceListContainer,
                        mActiveInstancesList,
                        mInactiveInstancesList,
                        /* isInactiveListShowingSupplier= */ () -> false,
                        mCommandItem,
                        MIN_COMMAND_ITEM_HEIGHT_PX,
                        ITEM_PADDING_HEIGHT_PX,
                        /* registerResizeListener= */ false);
        listener.onGlobalLayout();

        // Verify layout params.
        LayoutParams params = (LayoutParams) mInstanceListContainer.getLayoutParams();
        assertEquals("Height is incorrect.", LayoutParams.WRAP_CONTENT, params.height);
        assertEquals("Weight is incorrect.", 0, params.weight, 0);
    }

    @Test
    public void testInstanceListGlobalLayoutListener_SwitchToScrollableList() {
        setInitialContainerLayoutParams();

        // Simulate a non-scrollable active instances list.
        setUpActiveList(/* measuredHeight= */ 200, /* verticalScrollRange= */ 200);

        // Run the GlobalLayoutListener callback.
        var listener =
                InstanceSwitcherCoordinator.addLayoutListeners(
                        mDialogView,
                        mTabHeaderRow,
                        mInstanceListContainer,
                        mActiveInstancesList,
                        mInactiveInstancesList,
                        /* isInactiveListShowingSupplier= */ () -> false,
                        mCommandItem,
                        MIN_COMMAND_ITEM_HEIGHT_PX,
                        ITEM_PADDING_HEIGHT_PX,
                        /* registerResizeListener= */ false);
        listener.onGlobalLayout();
        // Verify layout params.
        LayoutParams params = (LayoutParams) mInstanceListContainer.getLayoutParams();
        assertEquals("Height is incorrect.", LayoutParams.WRAP_CONTENT, params.height);
        assertEquals("Weight is incorrect.", 0, params.weight, 0);

        // Simulate switching to the inactive instances list, that adds the listener again.
        when(mInactiveListAdapter.getItemCount()).thenReturn(5);
        listener =
                InstanceSwitcherCoordinator.addLayoutListeners(
                        mDialogView,
                        mTabHeaderRow,
                        mInstanceListContainer,
                        mActiveInstancesList,
                        mInactiveInstancesList,
                        /* isInactiveListShowingSupplier= */ () -> true,
                        mCommandItem,
                        MIN_COMMAND_ITEM_HEIGHT_PX,
                        ITEM_PADDING_HEIGHT_PX,
                        /* registerResizeListener= */ false);
        listener.onGlobalLayout();

        // Verify layout params.
        params = (LayoutParams) mInstanceListContainer.getLayoutParams();
        assertEquals("Height is incorrect.", 0, params.height);
        assertEquals("Weight is incorrect.", 1, params.weight, 0);
    }

    @Test
    public void testInstanceListGlobalLayoutListener_InactiveListEmpty() {
        setInitialContainerLayoutParams();

        // Simulate an empty inactive instances list.
        when(mInactiveListAdapter.getItemCount()).thenReturn(0);

        // Run the GlobalLayoutListener callback
        var listener =
                InstanceSwitcherCoordinator.addLayoutListeners(
                        mDialogView,
                        mTabHeaderRow,
                        mInstanceListContainer,
                        mActiveInstancesList,
                        mInactiveInstancesList,
                        /* isInactiveListShowingSupplier= */ () -> true,
                        mCommandItem,
                        MIN_COMMAND_ITEM_HEIGHT_PX,
                        ITEM_PADDING_HEIGHT_PX,
                        /* registerResizeListener= */ false);
        listener.onGlobalLayout();

        // Verify layout params
        LayoutParams params = (LayoutParams) mInstanceListContainer.getLayoutParams();
        assertEquals("Height is incorrect.", LayoutParams.WRAP_CONTENT, params.height);
        assertEquals("Weight is incorrect.", 0, params.weight, 0);
    }

    @Test
    public void testInstanceListGlobalLayoutListener_SetsMinimumDialogHeight() {
        setInitialContainerLayoutParams();

        // Simulate an active list with 5 items and an inactive list with 2 items
        int activeCount = 5;
        int inactiveCount = 2;
        when(mActiveListAdapter.getItemCount()).thenReturn(activeCount);
        when(mInactiveListAdapter.getItemCount()).thenReturn(inactiveCount);

        // Calculate expected height:
        // nonLastItemHeight = 66 (MIN_COMMAND_ITEM_HEIGHT_PX + ITEM_PADDING_HEIGHT_PX)
        // activeListHeight = 394 (activeCount * nonLastItemHeight + MIN_COMMAND_ITEM_HEIGHT_PX)
        // inactiveListHeight = 240 ((inactiveCount - 1) * nonLastItemHeight +
        // MIN_COMMAND_ITEM_HEIGHT_PX)
        // maxListHeight = 394 (max(activeListHeight, inactiveListHeight))
        // overheadHeight = 68 (tabHeight + paddingTop + ITEM_PADDING_HEIGHT_PX)
        // expectedHeight = 462 (maxListHeight + overheadHeight)
        int expectedHeight = 462;

        // Run the GlobalLayoutListener callback.
        var listener =
                InstanceSwitcherCoordinator.addLayoutListeners(
                        mDialogView,
                        mTabHeaderRow,
                        mInstanceListContainer,
                        mActiveInstancesList,
                        mInactiveInstancesList,
                        /* isInactiveListShowingSupplier= */ () -> false,
                        mCommandItem,
                        MIN_COMMAND_ITEM_HEIGHT_PX,
                        ITEM_PADDING_HEIGHT_PX,
                        /* registerResizeListener= */ false);
        listener.onGlobalLayout();

        // Verify minimum height is set correctly
        assertEquals(expectedHeight, mDialogView.getMinimumHeight());
    }

    @Test
    public void testLayoutChangeListener_TriggersUpdateOnHeightChange() {
        LayoutParams initialLayoutParams = setInitialContainerLayoutParams();

        // Register OnLayoutChangeListener
        InstanceSwitcherCoordinator.addLayoutListeners(
                mDialogView,
                mTabHeaderRow,
                mInstanceListContainer,
                mActiveInstancesList,
                mInactiveInstancesList,
                /* isInactiveListShowingSupplier= */ () -> false,
                mCommandItem,
                MIN_COMMAND_ITEM_HEIGHT_PX,
                ITEM_PADDING_HEIGHT_PX,
                /* registerResizeListener= */ true);
        assertEquals("Weight should not change yet.", 1, initialLayoutParams.weight, 0);

        // Simulate a height change of the dialog view.
        mDialogView.layout(0, 0, 100, 800);

        // Verify that the instance list container params were updated in response (the active
        // list is not scrollable and the command item is not compressed, so it should wrap
        // content).
        LayoutParams params = (LayoutParams) mInstanceListContainer.getLayoutParams();
        assertEquals("Height is incorrect.", LayoutParams.WRAP_CONTENT, params.height);
        assertEquals("Weight is incorrect.", 0, params.weight, 0);
    }
}
