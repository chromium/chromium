// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.view.ViewTreeObserver;
import android.widget.FrameLayout;
import android.widget.LinearLayout.LayoutParams;

import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.tabs.TabLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InstanceSwitcherCoordinatorUnitTest {
    @Mock private View mDialogView;
    @Mock private TabLayout mTabHeaderRow;
    @Mock private FrameLayout mInstanceListContainer;
    @Mock private RecyclerView mActiveInstancesList;
    @Mock private RecyclerView mInactiveInstancesList;
    @Mock private View mCommandItem;
    @Mock private RecyclerView.Adapter mActiveListAdapter;
    @Mock private RecyclerView.Adapter mInactiveListAdapter;

    private static final int MIN_COMMAND_ITEM_HEIGHT_PX = 64;
    private static final int ITEM_PADDING_HEIGHT_PX = 2;

    @Before
    public void setup() {
        MockitoAnnotations.openMocks(this);
        when(mDialogView.getPaddingTop()).thenReturn(16);
        when(mTabHeaderRow.getMeasuredHeight()).thenReturn(50);
        when(mInstanceListContainer.getViewTreeObserver()).thenReturn(mock(ViewTreeObserver.class));
        when(mCommandItem.getVisibility()).thenReturn(View.VISIBLE);
        when(mCommandItem.getMeasuredHeight()).thenReturn(64);
        when(mActiveInstancesList.getAdapter()).thenReturn(mActiveListAdapter);
        when(mInactiveInstancesList.getAdapter()).thenReturn(mInactiveListAdapter);
    }

    @Test
    public void testInstanceListGlobalLayoutListener_NoOpWhenDefault() {
        // Simulate XML spec.
        var initialLayoutParams = new LayoutParams(LayoutParams.MATCH_PARENT, 0);
        initialLayoutParams.weight = 1;

        // Simulate a scrollable active instances list.
        when(mActiveInstancesList.getMeasuredHeight()).thenReturn(200);
        when(mActiveInstancesList.computeVerticalScrollRange()).thenReturn(250);
        when(mInstanceListContainer.getLayoutParams()).thenReturn(initialLayoutParams);

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
        verify(mInstanceListContainer, never()).setLayoutParams(any());
    }

    @Test
    public void testInstanceListGlobalLayoutListener_NonScrollableList() {
        // Simulate XML spec.
        var initialLayoutParams = new LayoutParams(LayoutParams.MATCH_PARENT, 0);
        initialLayoutParams.weight = 1;

        // Simulate a non-scrollable active instances list.
        when(mActiveInstancesList.getMeasuredHeight()).thenReturn(200);
        when(mActiveInstancesList.computeVerticalScrollRange()).thenReturn(200);
        when(mInstanceListContainer.getLayoutParams()).thenReturn(initialLayoutParams);

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
        ArgumentCaptor<LayoutParams> paramsCaptor = ArgumentCaptor.forClass(LayoutParams.class);
        verify(mInstanceListContainer).setLayoutParams(paramsCaptor.capture());
        assertEquals(
                "Height is incorrect.", LayoutParams.WRAP_CONTENT, paramsCaptor.getValue().height);
        assertEquals("Weight is incorrect.", 0, paramsCaptor.getValue().weight, 0);
    }

    @Test
    public void testInstanceListGlobalLayoutListener_SwitchToScrollableList() {
        // Simulate XML spec.
        var initialLayoutParams = new LayoutParams(LayoutParams.MATCH_PARENT, 0);
        initialLayoutParams.weight = 1;

        // Simulate a non-scrollable active instances list.
        when(mActiveInstancesList.getMeasuredHeight()).thenReturn(200);
        when(mActiveInstancesList.computeVerticalScrollRange()).thenReturn(200);
        when(mInstanceListContainer.getLayoutParams()).thenReturn(initialLayoutParams);

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
        ArgumentCaptor<LayoutParams> paramsCaptor = ArgumentCaptor.forClass(LayoutParams.class);
        verify(mInstanceListContainer).setLayoutParams(paramsCaptor.capture());
        LayoutParams capturedParams = paramsCaptor.getValue();
        assertEquals("Height is incorrect.", LayoutParams.WRAP_CONTENT, capturedParams.height);
        assertEquals("Weight is incorrect.", 0, capturedParams.weight, 0);

        when(mInstanceListContainer.getLayoutParams()).thenReturn(capturedParams);

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
        verify(mInstanceListContainer, times(2)).setLayoutParams(paramsCaptor.capture());
        assertEquals("Height is incorrect.", 0, paramsCaptor.getValue().height);
        assertEquals("Weight is incorrect.", 1, paramsCaptor.getValue().weight, 0);
    }

    @Test
    public void testInstanceListGlobalLayoutListener_InactiveListEmpty() {
        // Simulate XML spec.
        var initialLayoutParams = new LayoutParams(LayoutParams.MATCH_PARENT, 0);
        initialLayoutParams.weight = 1;
        when(mInstanceListContainer.getLayoutParams()).thenReturn(initialLayoutParams);

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
        ArgumentCaptor<LayoutParams> paramsCaptor = ArgumentCaptor.forClass(LayoutParams.class);
        verify(mInstanceListContainer).setLayoutParams(paramsCaptor.capture());
        assertEquals(
                "Height is incorrect.", LayoutParams.WRAP_CONTENT, paramsCaptor.getValue().height);
        assertEquals("Weight is incorrect.", 0, paramsCaptor.getValue().weight, 0);
    }

    @Test
    public void testInstanceListGlobalLayoutListener_SetsMinimumDialogHeight() {
        // Simulate XML spec.
        var initialLayoutParams = new LayoutParams(LayoutParams.MATCH_PARENT, 0);
        initialLayoutParams.weight = 1;
        when(mInstanceListContainer.getLayoutParams()).thenReturn(initialLayoutParams);

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
        verify(mDialogView).setMinimumHeight(expectedHeight);
    }

    @Test
    public void testLayoutChangeListener_TriggersUpdateOnHeightChange() {
        // Simulate XML spec.
        var initialLayoutParams = new LayoutParams(LayoutParams.MATCH_PARENT, 0);
        initialLayoutParams.weight = 1;
        when(mInstanceListContainer.getLayoutParams()).thenReturn(initialLayoutParams);

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

        // Verify listener was added and capture it
        ArgumentCaptor<View.OnLayoutChangeListener> listenerCaptor =
                ArgumentCaptor.forClass(View.OnLayoutChangeListener.class);
        verify(mDialogView).addOnLayoutChangeListener(listenerCaptor.capture());
        View.OnLayoutChangeListener listener = listenerCaptor.getValue();

        // Simulate a height change (e.g., bottom changes from 300 to 600)
        listener.onLayoutChange(mDialogView, 0, 0, 100, 800, 0, 0, 100, 400);

        // Verify (indirectly via getLayoutParams) that maybeUpdateInstanceListContainerParams was
        // triggered in response
        verify(mInstanceListContainer).getLayoutParams();
    }
}
