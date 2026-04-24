// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.annotation.Px;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiContainerProperties;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;

/** Unit tests for {@link SideUiCoordinatorImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SideUiCoordinatorImplTest {

    private static final Size SIDE_UI_PARENT_SIZE = new Size(1000, 1000);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ViewStub mStartAnchorContainerStub;
    @Mock private ViewStub mEndAnchorContainerStub;
    @Mock private SideUiObserver mSideUiObserver;

    private final SettableNonNullObservableSupplier<Integer> mTopMarginSupplier =
            ObservableSuppliers.createNonNull(0);

    private FrameLayout mSideUiParent;
    private ViewGroup mStartAnchorContainer;
    private ViewGroup mEndAnchorContainer;
    private View mSideUiContainerView;
    private TestSideUiContainer mSideUiContainer;
    private SideUiCoordinatorImpl mCoordinator;

    @Before
    public void setUp() {
        Context context = Robolectric.buildActivity(Activity.class).setup().get();

        // Set up side UI's parent View.
        // Note that we simulate a measure pass and a layout pass, like what Android framework does.
        mSideUiParent = new FrameLayout(context);
        mSideUiParent.measure(
                View.MeasureSpec.makeMeasureSpec(
                        SIDE_UI_PARENT_SIZE.getWidth(), View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(
                        SIDE_UI_PARENT_SIZE.getHeight(), View.MeasureSpec.EXACTLY));
        mSideUiParent.layout(0, 0, SIDE_UI_PARENT_SIZE.getWidth(), SIDE_UI_PARENT_SIZE.getHeight());

        // Set up anchor containers.
        mStartAnchorContainer =
                (ViewGroup)
                        LayoutInflater.from(context)
                                .inflate(R.layout.side_ui_anchor_container, /* root= */ null);
        mEndAnchorContainer =
                (ViewGroup)
                        LayoutInflater.from(context)
                                .inflate(R.layout.side_ui_anchor_container, /* root= */ null);
        mSideUiParent.addView(mStartAnchorContainer);
        mSideUiParent.addView(mEndAnchorContainer);

        mSideUiContainerView = new View(context);
        mSideUiContainer = new TestSideUiContainer(mSideUiContainerView);

        doReturn(mStartAnchorContainer).when(mStartAnchorContainerStub).inflate();
        doReturn(mEndAnchorContainer).when(mEndAnchorContainerStub).inflate();

        // Initialize the SideUiCoordinator under test.
        mCoordinator =
                new SideUiCoordinatorImpl(
                        mStartAnchorContainerStub, mEndAnchorContainerStub, mTopMarginSupplier);
    }

    @Test
    public void testRegisterSideUiContainer() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);
        assertEquals(
                "Unexpected registered SideUiContainer.",
                mCoordinator.getSideUiContainerForTesting(),
                mSideUiContainer);

        mCoordinator.unregisterSideUiContainer(mSideUiContainer);
        assertNull(
                "Registered SideUiContainer expected to be null.",
                mCoordinator.getSideUiContainerForTesting());
    }

    @Test
    public void testAddObserver_NotifyCurrentSpecs() {
        int startContainerWidth = 25;
        int endContainerWidth = 75;
        mStartAnchorContainer.setMinimumWidth(startContainerWidth);
        mEndAnchorContainer.setMinimumWidth(endContainerWidth);

        // Add the observer after requesting an update.
        mCoordinator.addObserver(mSideUiObserver);

        // Verify the observer is still notified.
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(startContainerWidth, endContainerWidth);
        verify(mSideUiObserver).onSideUiSpecsChanged(expectedSideUiSpecs);
    }

    @Test
    public void testRemoveObserver_NotifyEmptySpecs() {
        mCoordinator.addObserver(mSideUiObserver);
        clearInvocations(mSideUiObserver);

        // Verify the observer is notified with empty specs on removal.
        mCoordinator.removeObserver(mSideUiObserver);
        verify(mSideUiObserver).onSideUiSpecsChanged(SideUiSpecs.EMPTY_SIDE_UI_SPECS);
    }

    @Test
    public void testRequestUpdateContainer_Start() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);
        mCoordinator.addObserver(mSideUiObserver);
        clearInvocations(mSideUiObserver);

        int width = 100;
        mCoordinator.requestUpdateContainer(new SideUiContainerProperties(AnchorSide.START, width));

        // Verify observers notified.
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(width, /* endContainerWidth= */ 0);
        verify(mSideUiObserver).onSideUiSpecsChanged(eq(expectedSideUiSpecs));

        // Verify view attached to start container.
        assertEquals(mStartAnchorContainer, mSideUiContainerView.getParent());
        assertEquals(width, getSideUiContainerViewWidth());
    }

    @Test
    public void testRequestUpdateContainer_End() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);
        mCoordinator.addObserver(mSideUiObserver);
        clearInvocations(mSideUiObserver);

        int width = 200;
        mCoordinator.requestUpdateContainer(new SideUiContainerProperties(AnchorSide.END, width));

        // Verify observers notified.
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(/* startContainerWidth= */ 0, width);
        verify(mSideUiObserver).onSideUiSpecsChanged(eq(expectedSideUiSpecs));

        // Verify view attached to end container.
        assertEquals(mEndAnchorContainer, mSideUiContainerView.getParent());
        assertEquals(width, getSideUiContainerViewWidth());
    }

    @Test
    public void testRequestUpdateContainer_DetachOnZeroWidth() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);

        // First attach.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.START, /* width= */ 100));
        assertEquals(mStartAnchorContainer, mSideUiContainerView.getParent());

        // Then update to width 0.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.START, /* width= */ 0));
        assertNull(mSideUiContainerView.getParent());
        assertEquals(0, getSideUiContainerViewWidth());
    }

    @Test
    public void testSwitchAnchorSides() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);

        // Start at START.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.START, /* width= */ 100));
        assertEquals(mStartAnchorContainer, mSideUiContainerView.getParent());

        // Switch to END.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.END, /* width= */ 200));
        assertEquals(mEndAnchorContainer, mSideUiContainerView.getParent());
    }

    @Test
    public void testAnchorContainerVisibility() {
        String unexpectedStart = "Unexpected start container visibility.";
        String unexpectedEnd = "Unexpected end container visibility.";
        mCoordinator.registerSideUiContainer(mSideUiContainer);

        // Verify starting visibility.
        assertEquals(unexpectedStart, View.GONE, mStartAnchorContainer.getVisibility());
        assertEquals(unexpectedEnd, View.GONE, mEndAnchorContainer.getVisibility());

        // Start at START.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.START, /* width= */ 10));
        assertEquals(unexpectedStart, View.VISIBLE, mStartAnchorContainer.getVisibility());
        assertEquals(unexpectedEnd, View.GONE, mEndAnchorContainer.getVisibility());

        // Switch to END.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.END, /* width= */ 90));
        assertEquals(unexpectedStart, View.GONE, mStartAnchorContainer.getVisibility());
        assertEquals(unexpectedEnd, View.VISIBLE, mEndAnchorContainer.getVisibility());

        // Detach.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.END, /* width= */ 0));
        assertEquals(unexpectedStart, View.GONE, mStartAnchorContainer.getVisibility());
        assertEquals(unexpectedEnd, View.GONE, mEndAnchorContainer.getVisibility());
    }

    @Test
    public void testOnTopMarginChanged() {
        // Set initial params, since these Views aren't actually attached.
        mStartAnchorContainer.setLayoutParams(new MarginLayoutParams(0, 0));
        mEndAnchorContainer.setLayoutParams(new MarginLayoutParams(0, 0));

        // Notify of a top margin change.
        @Px int topMarginPx = 30;
        mTopMarginSupplier.set(topMarginPx);

        // Verify the topMargin is set appropriately.
        MarginLayoutParams startLayoutParams =
                ((MarginLayoutParams) mStartAnchorContainer.getLayoutParams());
        assertEquals("Unexpected top margin.", topMarginPx, startLayoutParams.topMargin);

        MarginLayoutParams endLayoutParams =
                ((MarginLayoutParams) mEndAnchorContainer.getLayoutParams());
        assertEquals("Unexpected top margin.", topMarginPx, endLayoutParams.topMargin);
    }

    @Test
    public void testGetCurrentSideUiSpecs_AfterTopMarginChange() {
        int sideUiTopMargin = 100;
        mTopMarginSupplier.set(sideUiTopMargin);

        mCoordinator.getCurrentSideUiSpecs();

        assertEquals(
                "Unexpected measured height.",
                SIDE_UI_PARENT_SIZE.getHeight() - sideUiTopMargin,
                mStartAnchorContainer.getMeasuredHeight());
        assertEquals(
                "Unexpected measured height.",
                SIDE_UI_PARENT_SIZE.getHeight() - sideUiTopMargin,
                mEndAnchorContainer.getMeasuredHeight());
    }

    @Test
    public void testGetCurrentSideUiSpecs_AfterParentResize() {
        // Simulate the measure pass for when side UI's parent is resized.
        int newParentHeight = mSideUiParent.getHeight() - 100;
        mSideUiParent.measure(
                View.MeasureSpec.makeMeasureSpec(
                        mSideUiParent.getWidth(), View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(newParentHeight, View.MeasureSpec.EXACTLY));

        // Call getCurrentSideUiSpecs() before the layout pass.
        mCoordinator.getCurrentSideUiSpecs();

        // The anchor containers should use newParentHeight (the new measured height).
        assertEquals(newParentHeight, mStartAnchorContainer.getMeasuredHeight());
        assertEquals(newParentHeight, mEndAnchorContainer.getMeasuredHeight());

        // Now simulate the layout pass.
        mSideUiParent.layout(0, 0, SIDE_UI_PARENT_SIZE.getWidth(), newParentHeight);

        // Call getCurrentSideUiSpecs() again.
        mCoordinator.getCurrentSideUiSpecs();

        // The anchor containers' measured height should remain unchanged.
        assertEquals(newParentHeight, mStartAnchorContainer.getMeasuredHeight());
        assertEquals(newParentHeight, mEndAnchorContainer.getMeasuredHeight());
    }

    private int getSideUiContainerViewWidth() {
        return mSideUiContainerView.getLayoutParams().width;
    }
}
