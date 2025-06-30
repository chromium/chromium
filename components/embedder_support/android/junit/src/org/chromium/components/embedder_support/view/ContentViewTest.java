// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.view;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.openMocks;

import android.content.Context;
import android.os.Build;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.view.ViewStructure;
import android.view.autofill.AutofillValue;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.chromium.base.DeviceInfo;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.MotionEventTestUtils;
import org.chromium.ui.base.ViewAndroidDelegate;

/**
 * Unit tests for ContentView: the view which displays web content in Chrome. Unlike other tests for
 * ContentView (like ContentViewScrollingTest and ContentViewPointerTypeTest), these tests can
 * directly access the ContentView class.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ContentViewTest {
    @Mock private WebContents mWebContents;
    @Mock private ViewAndroidDelegate mViewDelegate;

    private Context mContext;
    private ContentView mContentView;

    @Before
    public void setUp() {
        openMocks(this);
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mContentView = new ContentView(mContext, mWebContents);
    }

    @Test
    @SmallTest
    public void testHandwritingHoverIconShowsWhenOverridden() {
        PointerIcon pointerIcon = PointerIcon.getSystemIcon(mContext, PointerIcon.TYPE_GRAB);
        mContentView.setStylusWritingIconSupplier(() -> pointerIcon);
        assertEquals(pointerIcon, mContentView.onResolvePointerIcon(null, 0));
    }

    @Test
    @SmallTest
    public void testOnResolvePointerIconCallsParentWhenNotOverridden() {
        mContentView.setStylusWritingIconSupplier(() -> null);
        MotionEvent motionEvent = mock(MotionEvent.class);
        assertNull(mContentView.onResolvePointerIcon(motionEvent, 0));
        // Parent implementation gets location of motion event.
        verify(motionEvent, atLeastOnce()).getX(0);
    }

    @Test
    @SmallTest
    public void testOnProvideAutofillVirtualStructureForwardsToDelegate() {
        when(mWebContents.getViewAndroidDelegate()).thenReturn(mViewDelegate);
        when(mViewDelegate.providesAutofillStructure()).thenReturn(true);
        ViewStructure structure = mock(ViewStructure.class);
        mContentView.onProvideAutofillVirtualStructure(
                structure, View.AUTOFILL_FLAG_INCLUDE_NOT_IMPORTANT_VIEWS);
        verify(mViewDelegate)
                .onProvideAutofillVirtualStructure(
                        structure, View.AUTOFILL_FLAG_INCLUDE_NOT_IMPORTANT_VIEWS);
    }

    @Test
    @SmallTest
    public void testForwardsAutofillDataToDelegate() {
        when(mWebContents.getViewAndroidDelegate()).thenReturn(mViewDelegate);
        when(mViewDelegate.providesAutofillStructure()).thenReturn(true);
        SparseArray<AutofillValue> values = new SparseArray();
        mContentView.autofill(values);
        verify(mViewDelegate).autofill(values);
    }

    private MotionEvent getTwoFingerSwipeTrackpadEvent(int action) {
        // Use Mockito.spy to override the return value of getClassification(). For some reason,
        // MotionEvent.obtain() ignores the specified classification value when in unit tests.
        // b/424066383.
        MotionEvent event = spy(MotionEventTestUtils.getTrackpadEvent(action, 0));
        when(event.getClassification()).thenReturn(MotionEvent.CLASSIFICATION_TWO_FINGER_SWIPE);
        return event;
    }

    @Test
    @SmallTest
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testIgnoreFakeTwoFingerSwipeEventsOnDesktop() {
        // This is a test for desktop-only hack.
        if (!DeviceInfo.isDesktop()) {
            return;
        }
        EventForwarder forwarder = mock(EventForwarder.class);
        GestureListenerManager gestureManager = mock(GestureListenerManager.class);

        // ACTION_DOWN followed by ACTION_CANCEL with no active fling scroll. ACTION_DOWN and
        // ACTION_CANCEL are consumed and not sent to EventForwarder.
        when(gestureManager.hasActiveFlingScroll()).thenReturn(false);
        assertTrue(
                mContentView.maybeHandleTwoFingerSwipeEvent(
                        getTwoFingerSwipeTrackpadEvent(MotionEvent.ACTION_DOWN),
                        forwarder,
                        gestureManager));
        assertTrue(
                mContentView.maybeHandleTwoFingerSwipeEvent(
                        getTwoFingerSwipeTrackpadEvent(MotionEvent.ACTION_CANCEL),
                        forwarder,
                        gestureManager));
        verify(forwarder, never()).onTouchEvent(any());

        // ACTION_DOWN followed by ACTION_CANCEL with an active fling scroll. ACTION_DOWN gets sent
        // to EventForwarder, and ACTION_CANCEL is not consumed.
        when(gestureManager.hasActiveFlingScroll()).thenReturn(true);
        assertTrue(
                mContentView.maybeHandleTwoFingerSwipeEvent(
                        getTwoFingerSwipeTrackpadEvent(MotionEvent.ACTION_DOWN),
                        forwarder,
                        gestureManager));
        assertFalse(
                mContentView.maybeHandleTwoFingerSwipeEvent(
                        getTwoFingerSwipeTrackpadEvent(MotionEvent.ACTION_CANCEL),
                        forwarder,
                        gestureManager));
        verify(forwarder, times(1)).onTouchEvent(any());

        // ACTION_DOWN followed by ACTION_MOVE. ACTION_DOWN gets sent to EventForwarder, and
        // ACTION_MOVE is not consumed.
        assertTrue(
                mContentView.maybeHandleTwoFingerSwipeEvent(
                        getTwoFingerSwipeTrackpadEvent(MotionEvent.ACTION_DOWN),
                        forwarder,
                        gestureManager));
        assertFalse(
                mContentView.maybeHandleTwoFingerSwipeEvent(
                        getTwoFingerSwipeTrackpadEvent(MotionEvent.ACTION_MOVE),
                        forwarder,
                        gestureManager));
        verify(forwarder, times(2)).onTouchEvent(any());
    }
}
