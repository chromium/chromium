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
    @Mock private FrameLayout mInstanceListContainer;
    @Mock private RecyclerView mActiveInstancesList;
    @Mock private RecyclerView mInactiveInstancesList;
    @Mock private View mCommandItem;
    @Mock private RecyclerView.Adapter mAdapter;

    private static final int MIN_COMMAND_ITEM_HEIGHT_PX = 173;

    @Before
    public void setup() {
        MockitoAnnotations.openMocks(this);
        when(mInstanceListContainer.getViewTreeObserver()).thenReturn(mock(ViewTreeObserver.class));
        when(mCommandItem.getVisibility()).thenReturn(View.VISIBLE);
        when(mCommandItem.getMeasuredHeight()).thenReturn(200);
        when(mInactiveInstancesList.getAdapter()).thenReturn(mAdapter);
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
                InstanceSwitcherCoordinator.addInstanceListGlobalLayoutListener(
                        mInstanceListContainer,
                        mActiveInstancesList,
                        mInactiveInstancesList,
                        /* isInactiveListShowing= */ false,
                        mCommandItem,
                        MIN_COMMAND_ITEM_HEIGHT_PX);
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
                InstanceSwitcherCoordinator.addInstanceListGlobalLayoutListener(
                        mInstanceListContainer,
                        mActiveInstancesList,
                        mInactiveInstancesList,
                        /* isInactiveListShowing= */ false,
                        mCommandItem,
                        MIN_COMMAND_ITEM_HEIGHT_PX);
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
                InstanceSwitcherCoordinator.addInstanceListGlobalLayoutListener(
                        mInstanceListContainer,
                        mActiveInstancesList,
                        mInactiveInstancesList,
                        /* isInactiveListShowing= */ false,
                        mCommandItem,
                        MIN_COMMAND_ITEM_HEIGHT_PX);
        listener.onGlobalLayout();
        // Verify layout params.
        ArgumentCaptor<LayoutParams> paramsCaptor = ArgumentCaptor.forClass(LayoutParams.class);
        verify(mInstanceListContainer).setLayoutParams(paramsCaptor.capture());
        LayoutParams capturedParams = paramsCaptor.getValue();
        assertEquals("Height is incorrect.", LayoutParams.WRAP_CONTENT, capturedParams.height);
        assertEquals("Weight is incorrect.", 0, capturedParams.weight, 0);

        when(mInstanceListContainer.getLayoutParams()).thenReturn(capturedParams);

        // Simulate switching to the inactive instances list, that adds the listener again.
        when(mAdapter.getItemCount()).thenReturn(5);
        listener =
                InstanceSwitcherCoordinator.addInstanceListGlobalLayoutListener(
                        mInstanceListContainer,
                        mActiveInstancesList,
                        mInactiveInstancesList,
                        /* isInactiveListShowing= */ true,
                        mCommandItem,
                        MIN_COMMAND_ITEM_HEIGHT_PX);
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
        when(mAdapter.getItemCount()).thenReturn(0);

        // Run the GlobalLayoutListener callback
        var listener =
                InstanceSwitcherCoordinator.addInstanceListGlobalLayoutListener(
                        mInstanceListContainer,
                        mActiveInstancesList,
                        mInactiveInstancesList,
                        /* isInactiveListShowing= */ true,
                        mCommandItem,
                        MIN_COMMAND_ITEM_HEIGHT_PX);
        listener.onGlobalLayout();

        // Verify layout params
        ArgumentCaptor<LayoutParams> paramsCaptor = ArgumentCaptor.forClass(LayoutParams.class);
        verify(mInstanceListContainer).setLayoutParams(paramsCaptor.capture());
        assertEquals(
                "Height is incorrect.", LayoutParams.WRAP_CONTENT, paramsCaptor.getValue().height);
        assertEquals("Weight is incorrect.", 0, paramsCaptor.getValue().weight, 0);
    }
}
