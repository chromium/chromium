// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;

/** Tests for {@link ViewMarginAdjusterForSideUi}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ViewMarginAdjusterForSideUiTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock View mView;
    @Captor ArgumentCaptor<MarginLayoutParams> mLayoutParamsCaptor;

    private MarginLayoutParams mMarginLayoutParams;

    @Before
    public void setUp() {
        mMarginLayoutParams = new MarginLayoutParams(0, 0);
        doReturn(mMarginLayoutParams).when(mView).getLayoutParams();
    }

    @Test
    public void testOnSideUiSpecsChanged_noBaseMargin() {
        SideUiObserver marginContainerObserver = new ViewMarginAdjusterForSideUi(mView);

        // End margin
        marginContainerObserver.onSideUiSpecsChanged(new SideUiSpecs(0, 200));
        verify(mView).setLayoutParams(mLayoutParamsCaptor.capture());
        assertEquals(0, mLayoutParamsCaptor.getValue().getMarginStart());
        assertEquals(200, mLayoutParamsCaptor.getValue().getMarginEnd());

        // Start margin
        marginContainerObserver.onSideUiSpecsChanged(new SideUiSpecs(200, 0));
        verify(mView, times(2)).setLayoutParams(mLayoutParamsCaptor.capture());
        assertEquals(200, mLayoutParamsCaptor.getValue().getMarginStart());
        assertEquals(0, mLayoutParamsCaptor.getValue().getMarginEnd());

        // Both margins
        marginContainerObserver.onSideUiSpecsChanged(new SideUiSpecs(100, 200));
        verify(mView, times(3)).setLayoutParams(mLayoutParamsCaptor.capture());
        assertEquals(100, mLayoutParamsCaptor.getValue().getMarginStart());
        assertEquals(200, mLayoutParamsCaptor.getValue().getMarginEnd());
    }

    @Test
    public void testOnSideUiSpecsChanged_hasBaseMargin() {
        mMarginLayoutParams.setMarginStart(20);
        mMarginLayoutParams.setMarginEnd(35);

        SideUiObserver marginContainerObserver = new ViewMarginAdjusterForSideUi(mView);

        // End margin
        marginContainerObserver.onSideUiSpecsChanged(new SideUiSpecs(0, 200));
        verify(mView).setLayoutParams(mLayoutParamsCaptor.capture());
        assertEquals(20, mLayoutParamsCaptor.getValue().getMarginStart());
        assertEquals(235, mLayoutParamsCaptor.getValue().getMarginEnd());

        // Start margin
        marginContainerObserver.onSideUiSpecsChanged(new SideUiSpecs(200, 0));
        verify(mView, times(2)).setLayoutParams(mLayoutParamsCaptor.capture());
        assertEquals(220, mLayoutParamsCaptor.getValue().getMarginStart());
        assertEquals(35, mLayoutParamsCaptor.getValue().getMarginEnd());

        // Both margins
        marginContainerObserver.onSideUiSpecsChanged(new SideUiSpecs(100, 200));
        verify(mView, times(3)).setLayoutParams(mLayoutParamsCaptor.capture());
        assertEquals(120, mLayoutParamsCaptor.getValue().getMarginStart());
        assertEquals(235, mLayoutParamsCaptor.getValue().getMarginEnd());
    }
}
