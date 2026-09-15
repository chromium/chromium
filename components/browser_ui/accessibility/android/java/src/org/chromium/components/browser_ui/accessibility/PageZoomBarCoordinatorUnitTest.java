// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/** Unit tests for {@link PageZoomBarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PageZoomBarCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PageZoomBarCoordinatorDelegate mDelegateMock;
    @Mock private PageZoomManager mPageZoomManagerMock;
    @Mock private BottomSheetController mBottomSheetControllerMock;

    @Mock(extraInterfaces = WebContentsObserver.Observable.class)
    private WebContents mWebContentsMock;

    @Captor private ArgumentCaptor<BottomSheetObserver> mObserverCaptor;

    private SettableMonotonicObservableSupplier<BottomSheetController>
            mBottomSheetControllerSupplier;
    private PageZoomBarCoordinator mCoordinator;
    private View mRealView;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_MaterialComponents_DayNight);

        // Inflate real layout so the ViewBinder does not crash on findViewById calls
        mRealView = LayoutInflater.from(mContext).inflate(R.layout.page_zoom_view, null);
        // Ensure the view has MarginLayoutParams
        mRealView.setLayoutParams(
                new MarginLayoutParams(
                        MarginLayoutParams.MATCH_PARENT, MarginLayoutParams.WRAP_CONTENT));

        when(mDelegateMock.getZoomControlView()).thenReturn(mRealView);

        mBottomSheetControllerSupplier = ObservableSuppliers.createMonotonic();

        mCoordinator =
                new PageZoomBarCoordinator(
                        mDelegateMock,
                        mPageZoomManagerMock,
                        /* useSlider= */ true,
                        mBottomSheetControllerSupplier);

        // Initialize the supplier
        mBottomSheetControllerSupplier.set(mBottomSheetControllerMock);

        // Capture the registered observer
        verify(mBottomSheetControllerMock, atLeastOnce()).addObserver(mObserverCaptor.capture());
    }

    @Test
    public void testTranslationAdjustment_withBottomSheetOffset() {
        mCoordinator.show(mWebContentsMock);

        // Scenario 1: No sheet offset, bottom controls offset = 100
        mCoordinator.onBottomControlsHeightChanged(100);
        when(mBottomSheetControllerMock.getCurrentOffset()).thenReturn(0);

        // Trigger offset change
        mObserverCaptor.getValue().onSheetOffsetChanged(0.0f, 0.0f);
        assertEquals(-100.0f, mRealView.getTranslationY(), 0.0f);

        // Scenario 2: Bottom sheet offset = 150 (exceeds bottom controls 100)
        when(mBottomSheetControllerMock.getCurrentOffset()).thenReturn(150);
        mObserverCaptor.getValue().onSheetOffsetChanged(0.5f, 150.0f);
        assertEquals(-150.0f, mRealView.getTranslationY(), 0.0f);
    }

    @Test
    public void testDismissZoomBar_whenBottomSheetOpened() {
        mCoordinator.show(mWebContentsMock);
        assertEquals(View.VISIBLE, mRealView.getVisibility());

        // Simulate bottom sheet opening (expanded beyond peek)
        mObserverCaptor.getValue().onSheetOpened(0);
        assertEquals(View.GONE, mRealView.getVisibility());
    }

    @Test
    public void testNoShow_whenBottomSheetAlreadyOpen() {
        // Setup: Bottom sheet is already open
        when(mBottomSheetControllerMock.isSheetOpen()).thenReturn(true);

        mCoordinator.show(mWebContentsMock);

        // Verify that the view was never inflated/shown
        verify(mDelegateMock, never()).getZoomControlView();
    }

    @Test
    public void testDismissZoomBar_onConstruction_whenBottomSheetAlreadyOpen() {
        // Setup: Bottom sheet is open prior to coordinator initialization
        BottomSheetController mockController = mock(BottomSheetController.class);
        when(mockController.isSheetOpen()).thenReturn(true);

        SettableMonotonicObservableSupplier<BottomSheetController> supplier =
                ObservableSuppliers.createMonotonic();

        PageZoomBarCoordinator coordinator =
                new PageZoomBarCoordinator(mDelegateMock, mPageZoomManagerMock, true, supplier);

        // Trigger supplier initialization
        supplier.set(mockController);

        // Verify early dismissal was evaluated
        verify(mockController).isSheetOpen();
    }

    @Test
    public void testTranslationAdjustment_withBottomSheetAnchored() {
        mCoordinator.show(mWebContentsMock);

        // Scenario 1: Anchored, but NOT acting as browser controls.
        // Should sum the offsets: bottom controls (100) + sheet offset (50) = 150.
        mCoordinator.onBottomControlsHeightChanged(100);
        when(mBottomSheetControllerMock.getCurrentOffset()).thenReturn(50);
        when(mBottomSheetControllerMock.isAnchoredToBottomControls()).thenReturn(true);
        when(mDelegateMock.isSheetActingAsBrowserControls()).thenReturn(false);

        mObserverCaptor.getValue().onSheetOffsetChanged(0.5f, 50.0f);
        assertEquals(-150.0f, mRealView.getTranslationY(), 0.0f);

        // Scenario 2: Anchored, AND acting as browser controls.
        // Should only use bottom controls offset: bottom controls (100).
        // (sheet offset is ignored/already included in bottom controls).
        when(mDelegateMock.isSheetActingAsBrowserControls()).thenReturn(true);
        mObserverCaptor.getValue().onSheetOffsetChanged(0.5f, 50.0f);
        assertEquals(-100.0f, mRealView.getTranslationY(), 0.0f);
    }
}
