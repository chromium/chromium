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
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.Activity;
import android.content.res.Configuration;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
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
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiContainerProperties;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.ViewUtils;

/** Unit tests for {@link SideUiCoordinatorImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "w1920dp-h1080dp-mdpi")
public class SideUiCoordinatorImplTest {
    /** Window size in this test; it must match {@code @Config}. */
    private static final Size WINDOW_SIZE_PX = new Size(1920, 1080);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private ViewStub mStartAnchorContainerStub;
    @Mock private ViewStub mEndAnchorContainerStub;
    @Mock private SideUiObserver mSideUiObserver;

    private final SettableNonNullObservableSupplier<Integer> mTopMarginSupplier =
            ObservableSuppliers.createNonNull(0);

    private Activity mTestActivity;
    private FrameLayout mAnchorContainerParent;
    private ViewGroup mStartAnchorContainer;
    private ViewGroup mEndAnchorContainer;
    private View mSideUiContainerView;
    private TestSideUiContainer mSideUiContainer;
    private SideUiCoordinatorImpl mCoordinator;

    @Before
    public void setUp() {
        mTestActivity = Robolectric.buildActivity(TestActivity.class).setup().get();

        // Set up the parent View of side UI anchor containers.
        mAnchorContainerParent = new FrameLayout(mTestActivity);
        mTestActivity.addContentView(
                mAnchorContainerParent,
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        // Set up anchor containers.
        mStartAnchorContainer =
                (ViewGroup)
                        LayoutInflater.from(mTestActivity)
                                .inflate(R.layout.side_ui_anchor_container, /* root= */ null);
        mEndAnchorContainer =
                (ViewGroup)
                        LayoutInflater.from(mTestActivity)
                                .inflate(R.layout.side_ui_anchor_container, /* root= */ null);
        mAnchorContainerParent.addView(
                mStartAnchorContainer,
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT));
        mAnchorContainerParent.addView(
                mEndAnchorContainer,
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT));

        mSideUiContainerView = new View(mTestActivity);
        mSideUiContainer = new TestSideUiContainer(mSideUiContainerView);

        doReturn(mStartAnchorContainer).when(mStartAnchorContainerStub).inflate();
        doReturn(mEndAnchorContainer).when(mEndAnchorContainerStub).inflate();

        // Initialize the SideUiCoordinator under test.
        mCoordinator =
                new SideUiCoordinatorImpl(
                        mTestActivity,
                        mActivityLifecycleDispatcher,
                        mAnchorContainerParent,
                        mStartAnchorContainerStub,
                        mEndAnchorContainerStub,
                        mTopMarginSupplier);

        // Make sure the measure pass and the layout pass are completed before running tests.
        RobolectricUtil.runAllBackgroundAndUi();

        // mAnchorContainerParent should have the size specified in @Config.
        assertEquals(WINDOW_SIZE_PX.getWidth(), mAnchorContainerParent.getWidth());
        assertEquals(WINDOW_SIZE_PX.getHeight(), mAnchorContainerParent.getHeight());
    }

    @Test
    public void testConstructor_RegisterListeners() {
        // The constructor is invoked in setUp().

        verify(mActivityLifecycleDispatcher).register(mCoordinator);
        assertEquals(1, mTopMarginSupplier.getObserverCount());
    }

    @Test
    public void testDestroy_UnregisterListeners() {
        mCoordinator.destroy();

        verify(mActivityLifecycleDispatcher).unregister(mCoordinator);
        assertEquals(0, mTopMarginSupplier.getObserverCount());
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
    public void testRequestUpdateContainer_AnchorSideIsStart() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);
        mCoordinator.addObserver(mSideUiObserver);
        clearInvocations(mSideUiObserver);

        int width = 100;
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.START, width),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify observers notified.
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(width, /* endContainerWidth= */ 0);
        verify(mSideUiObserver).onSideUiSpecsChanged(eq(expectedSideUiSpecs));

        // Verify view attached to start container.
        assertEquals(mStartAnchorContainer, mSideUiContainerView.getParent());
        assertEquals(width, getSideUiContainerViewWidth());
    }

    @Test
    public void testRequestUpdateContainer_AnchorSideIsEnd() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);
        mCoordinator.addObserver(mSideUiObserver);
        clearInvocations(mSideUiObserver);

        int width = 200;
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.END, width),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

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
                new SideUiContainerProperties(AnchorSide.START, /* width= */ 100),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(mStartAnchorContainer, mSideUiContainerView.getParent());

        // Then update to width 0.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.START, /* width= */ 0),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        assertNull(mSideUiContainerView.getParent());
        assertEquals(0, getSideUiContainerViewWidth());
    }

    @Test
    public void testRequestUpdateContainer_InvokeDetermineContainerWidth() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);

        int width = 200;
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.END, width),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify SideUiContainer#determineContainerWidth() is invoked with correct parameters.
        int minWebContentsWidthPx =
                ViewUtils.dpToPx(mTestActivity, SideUiCoordinator.MIN_WEB_CONTENTS_WIDTH_DP);
        assertEquals(Integer.valueOf(width), mSideUiContainer.mLastRequestedWidth);
        assertEquals(
                Integer.valueOf(WINDOW_SIZE_PX.getWidth() - minWebContentsWidthPx),
                mSideUiContainer.mLastAvailableWidth);
        assertEquals(Integer.valueOf(WINDOW_SIZE_PX.getWidth()), mSideUiContainer.mLastWindowWidth);
    }

    @Test
    public void testSwitchAnchorSides() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);

        // Start at START.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.START, /* width= */ 100),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(mStartAnchorContainer, mSideUiContainerView.getParent());

        // Switch to END.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.END, /* width= */ 200),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
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
                new SideUiContainerProperties(AnchorSide.START, /* width= */ 10),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(unexpectedStart, View.VISIBLE, mStartAnchorContainer.getVisibility());
        assertEquals(unexpectedEnd, View.GONE, mEndAnchorContainer.getVisibility());

        // Switch to END.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.END, /* width= */ 90),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(unexpectedStart, View.GONE, mStartAnchorContainer.getVisibility());
        assertEquals(unexpectedEnd, View.VISIBLE, mEndAnchorContainer.getVisibility());

        // Detach.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.END, /* width= */ 0),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
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
    public void testMeasureSideUiSpecs_AfterTopMarginChange() {
        int sideUiTopMargin = 100;
        mTopMarginSupplier.set(sideUiTopMargin);

        mCoordinator.measureSideUiSpecs();

        assertEquals(
                "Unexpected measured height.",
                mAnchorContainerParent.getHeight() - sideUiTopMargin,
                mStartAnchorContainer.getMeasuredHeight());
        assertEquals(
                "Unexpected measured height.",
                mAnchorContainerParent.getHeight() - sideUiTopMargin,
                mEndAnchorContainer.getMeasuredHeight());
    }

    @Test
    public void testMeasureSideUiSpecs_AfterParentResize() {
        // Simulate the measure pass for when side UI's parent is resized.
        int newParentHeight = mAnchorContainerParent.getHeight() - 100;
        mAnchorContainerParent.measure(
                View.MeasureSpec.makeMeasureSpec(
                        mAnchorContainerParent.getWidth(), View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(newParentHeight, View.MeasureSpec.EXACTLY));

        // Call measureSideUiSpecs() before the layout pass.
        mCoordinator.measureSideUiSpecs();

        // The anchor containers should use newParentHeight (the new measured height).
        assertEquals(newParentHeight, mStartAnchorContainer.getMeasuredHeight());
        assertEquals(newParentHeight, mEndAnchorContainer.getMeasuredHeight());

        // Now simulate the layout pass.
        mAnchorContainerParent.layout(0, 0, mAnchorContainerParent.getWidth(), newParentHeight);

        // Call measureSideUiSpecs() again.
        mCoordinator.measureSideUiSpecs();

        // The anchor containers' measured height should remain unchanged.
        assertEquals(newParentHeight, mStartAnchorContainer.getMeasuredHeight());
        assertEquals(newParentHeight, mEndAnchorContainer.getMeasuredHeight());
    }

    @Test
    public void testOnConfigurationChanged_SideUiSpecsChanged_ApplyNewSpecs() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);
        mCoordinator.addObserver(mSideUiObserver);

        // Open a side UI.
        @Px int initialSideUiWidth = 360;
        mSideUiContainer.mDeterminedWidth = initialSideUiWidth;
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.END, initialSideUiWidth),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        // Simulate a configuration change.
        clearInvocations(mSideUiObserver);
        @Px int newSideUiWidth = 412;
        mSideUiContainer.mDeterminedWidth = newSideUiWidth;
        mCoordinator.onConfigurationChanged(new Configuration());
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify that observers are notified with the updated specs.
        SideUiSpecs expectedSideUiSpecs =
                new SideUiSpecs(/* startContainerWidth= */ 0, newSideUiWidth);
        verify(mSideUiObserver).onSideUiSpecsChanged(eq(expectedSideUiSpecs));

        // Verify the container view's width is updated.
        assertEquals(newSideUiWidth, getSideUiContainerViewWidth());
    }

    @Test
    public void testOnConfigurationChanged_SideUiSpecsNotChanged_NoOp() {
        mCoordinator.registerSideUiContainer(mSideUiContainer);
        mCoordinator.addObserver(mSideUiObserver);

        // Open a side UI.
        @Px int initialSideUiWidth = 360;
        mSideUiContainer.mDeterminedWidth = initialSideUiWidth;
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(AnchorSide.END, initialSideUiWidth),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        // Simulate a configuration change.
        // Note that we don't change "mSideUiContainer.mDeterminedWidth", which means the
        // SideUiSpecs remains unchanged.
        clearInvocations(mSideUiObserver);
        mCoordinator.onConfigurationChanged(new Configuration());
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify that observers are NOT notified again (no new changes).
        verifyNoInteractions(mSideUiObserver);

        // Verify the container view's width is unchanged.
        assertEquals(initialSideUiWidth, getSideUiContainerViewWidth());
    }

    private int getSideUiContainerViewWidth() {
        return mSideUiContainerView.getLayoutParams().width;
    }
}
