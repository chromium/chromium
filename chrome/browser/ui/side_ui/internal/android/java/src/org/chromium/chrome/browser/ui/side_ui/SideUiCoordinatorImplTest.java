// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import static org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.MIN_WEB_CONTENTS_WIDTH_DP;
import static org.chromium.chrome.browser.ui.side_ui.TestSideUiContainer.TEST_SIDE_UI_WIDTH;

import android.app.Activity;
import android.content.res.Configuration;
import android.util.ArrayMap;
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
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiContainerProperties;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.ViewUtils;

import java.util.Map;

/** Unit tests for {@link SideUiCoordinatorImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "w1920dp-h1080dp-mdpi" /* windowWidth = 1920dp; 1920dp = 1920px (mdpi) */)
public class SideUiCoordinatorImplTest {
    private static final @Px int WINDOW_WIDTH = 1920;

    /** Window size in this test; it must match {@code @Config}. */
    private static final Size WINDOW_SIZE_PX = new Size(WINDOW_WIDTH, 1080);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private ViewStub mLeftAnchorContainerStub;
    @Mock private ViewStub mRightAnchorContainerStub;
    @Mock private SideUiObserver mSideUiObserver;

    private final SettableNonNullObservableSupplier<Integer> mTopMarginSupplier =
            ObservableSuppliers.createNonNull(0);

    private Activity mTestActivity;
    private FrameLayout mAnchorContainerParent;
    private ViewGroup mLeftAnchorContainer;
    private ViewGroup mRightAnchorContainer;
    private View mSideUiContainerView;
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
        mLeftAnchorContainer =
                (ViewGroup)
                        LayoutInflater.from(mTestActivity)
                                .inflate(R.layout.side_ui_anchor_container, /* root= */ null);
        mRightAnchorContainer =
                (ViewGroup)
                        LayoutInflater.from(mTestActivity)
                                .inflate(R.layout.side_ui_anchor_container, /* root= */ null);
        mAnchorContainerParent.addView(
                mLeftAnchorContainer,
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT));
        mAnchorContainerParent.addView(
                mRightAnchorContainer,
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT));

        doReturn(mLeftAnchorContainer).when(mLeftAnchorContainerStub).inflate();
        doReturn(mRightAnchorContainer).when(mRightAnchorContainerStub).inflate();

        // Initialize the SideUiCoordinator under test.
        mCoordinator =
                new SideUiCoordinatorImpl(
                        mTestActivity,
                        mActivityLifecycleDispatcher,
                        mAnchorContainerParent,
                        mLeftAnchorContainerStub,
                        mRightAnchorContainerStub,
                        mTopMarginSupplier);

        // Initialize the SideUiContainer View.
        mSideUiContainerView = new View(mTestActivity);

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
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.RIGHT);
        mCoordinator.registerSideUiContainer(sideUiContainer);
        assertEquals(
                "Unexpected registered SideUiContainer.",
                mCoordinator.getSideUiContainerForTesting(sideUiContainer.getSideUiId()),
                sideUiContainer);

        mCoordinator.unregisterSideUiContainer(sideUiContainer);
        assertNull(
                "Registered SideUiContainer expected to be null.",
                mCoordinator.getSideUiContainerForTesting(sideUiContainer.getSideUiId()));
    }

    @Test
    public void testRequestUpdateContainer_AnchorSideIsLeft() {
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.LEFT);
        mCoordinator.registerSideUiContainer(sideUiContainer);
        mCoordinator.addObserver(mSideUiObserver);
        clearInvocations(mSideUiObserver);

        int width = TEST_SIDE_UI_WIDTH;
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(
                        sideUiContainer.getSideUiId(), sideUiContainer.getAnchorSide(), width),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify observers notified.
        Map<Integer, Integer> sideUiWidths = new ArrayMap<>();
        sideUiWidths.put(AnchorSide.LEFT, width);
        sideUiWidths.put(AnchorSide.RIGHT, 0);
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(sideUiWidths);
        verify(mSideUiObserver).onSideUiSpecsChanged(eq(expectedSideUiSpecs));

        // Verify view attached to left container.
        assertEquals(mLeftAnchorContainer, mSideUiContainerView.getParent());
        assertEquals(width, getSideUiContainerViewWidth());
    }

    @Test
    public void testRequestUpdateContainer_AnchorSideIsRight() {
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.RIGHT);
        mCoordinator.registerSideUiContainer(sideUiContainer);
        mCoordinator.addObserver(mSideUiObserver);
        clearInvocations(mSideUiObserver);

        int width = TEST_SIDE_UI_WIDTH;
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(
                        sideUiContainer.getSideUiId(), sideUiContainer.getAnchorSide(), width),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify observers notified.
        Map<Integer, Integer> sideUiWidths = new ArrayMap<>();
        sideUiWidths.put(AnchorSide.LEFT, 0);
        sideUiWidths.put(AnchorSide.RIGHT, width);
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(sideUiWidths);
        verify(mSideUiObserver).onSideUiSpecsChanged(eq(expectedSideUiSpecs));

        // Verify view attached to right container.
        assertEquals(mRightAnchorContainer, mSideUiContainerView.getParent());
        assertEquals(width, getSideUiContainerViewWidth());
    }

    @Test
    public void testRequestUpdateContainer_hideLowerPriorityContainer() {
        View rightUiContainerView = new FrameLayout(mTestActivity);
        var rightUiContainer =
                new TestSideUiContainer(
                        mCoordinator,
                        rightUiContainerView,
                        SideUiId.SIDE_UI_FOR_TESTING_LOW_PRIORITY,
                        AnchorSide.RIGHT);
        mCoordinator.registerSideUiContainer(rightUiContainer);
        rightUiContainer.mMinWidthDp = 200;

        View leftUiContainerView = new FrameLayout(mTestActivity);
        var leftUiContainer =
                new TestSideUiContainer(
                        mCoordinator,
                        leftUiContainerView,
                        SideUiId.SIDE_UI_FOR_TESTING_HIGH_PRIORITY,
                        AnchorSide.LEFT);

        assertTrue(
                "Left container should have the higher priority",
                leftUiContainer.getSideUiId() < rightUiContainer.getSideUiId());

        mCoordinator.registerSideUiContainer(leftUiContainer);
        mCoordinator.addObserver(mSideUiObserver);
        clearInvocations(mSideUiObserver);

        int width = TEST_SIDE_UI_WIDTH;
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(
                        rightUiContainer.getSideUiId(), rightUiContainer.getAnchorSide(), width),
                /* suppressAnimations= */ true);
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(
                        leftUiContainer.getSideUiId(), leftUiContainer.getAnchorSide(), width),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(width, width);
        SideUiSpecs currentSideUiSpecs = mCoordinator.getCurrentSideUiSpecs();
        assertEquals(expectedSideUiSpecs, currentSideUiSpecs);
        assertEquals(width, leftUiContainerView.getLayoutParams().width);
        assertEquals(width, rightUiContainerView.getLayoutParams().width);

        // The left container requests width bigger than the right one can keep its current width.
        @Px
        int threshold =
                ViewUtils.dpToPx(
                        mTestActivity,
                        SideUiCoordinator.MIN_WEB_CONTENTS_WIDTH_DP + rightUiContainer.mMinWidthDp);
        @Px int leftWidth = WINDOW_WIDTH - threshold + 1;
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(
                        leftUiContainer.getSideUiId(), leftUiContainer.getAnchorSide(), leftWidth),
                /* suppressAnimations= */ true);

        // Verify the right container gets hidden.
        RobolectricUtil.runAllBackgroundAndUi();
        expectedSideUiSpecs = new SideUiSpecs(leftWidth, 0);
        currentSideUiSpecs = mCoordinator.getCurrentSideUiSpecs();
        assertEquals(expectedSideUiSpecs, currentSideUiSpecs);
        assertEquals(leftWidth, leftUiContainerView.getLayoutParams().width);
        assertEquals(0, rightUiContainerView.getLayoutParams().width);
    }

    @Test
    public void testRequestUpdateContainer_DetachOnZeroWidth() {
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.RIGHT);
        mCoordinator.registerSideUiContainer(sideUiContainer);

        // First attach.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(
                        sideUiContainer.getSideUiId(),
                        sideUiContainer.getAnchorSide(),
                        TEST_SIDE_UI_WIDTH),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(mRightAnchorContainer, mSideUiContainerView.getParent());

        // Then update to width 0.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(
                        sideUiContainer.getSideUiId(),
                        sideUiContainer.getAnchorSide(),
                        /* width= */ 0),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        assertNull(mSideUiContainerView.getParent());
        assertEquals(0, getSideUiContainerViewWidth());
    }

    @Test
    public void testRequestUpdateContainer_InvokeDetermineContainerWidth() {
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.RIGHT);
        mCoordinator.registerSideUiContainer(sideUiContainer);

        int width = TEST_SIDE_UI_WIDTH;
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(
                        sideUiContainer.getSideUiId(), sideUiContainer.getAnchorSide(), width),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify SideUiContainer#determineContainerWidth() is invoked with correct parameters.
        int minWebContentsWidthPx = ViewUtils.dpToPx(mTestActivity, MIN_WEB_CONTENTS_WIDTH_DP);
        assertEquals(Integer.valueOf(width), sideUiContainer.mLastRequestedWidth);
        assertEquals(
                Integer.valueOf(WINDOW_SIZE_PX.getWidth() - minWebContentsWidthPx),
                sideUiContainer.mLastAvailableWidth);
        assertEquals(Integer.valueOf(WINDOW_SIZE_PX.getWidth()), sideUiContainer.mLastWindowWidth);
    }

    @Test
    public void testLeftAnchorContainerVisibility() {
        String unexpectedLeft = "Unexpected left container visibility.";
        String unexpectedRight = "Unexpected right container visibility.";
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.LEFT);
        mCoordinator.registerSideUiContainer(sideUiContainer);
        // Verify starting visibility.
        assertEquals(unexpectedLeft, View.GONE, mLeftAnchorContainer.getVisibility());
        assertEquals(unexpectedRight, View.GONE, mRightAnchorContainer.getVisibility());

        // Start at LEFT.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(
                        sideUiContainer.getSideUiId(),
                        sideUiContainer.getAnchorSide(),
                        TEST_SIDE_UI_WIDTH),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(unexpectedLeft, View.VISIBLE, mLeftAnchorContainer.getVisibility());
        assertEquals(unexpectedRight, View.GONE, mRightAnchorContainer.getVisibility());

        // Detach.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(
                        sideUiContainer.getSideUiId(),
                        sideUiContainer.getAnchorSide(),
                        /* width= */ 0),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(unexpectedLeft, View.GONE, mLeftAnchorContainer.getVisibility());
        assertEquals(unexpectedRight, View.GONE, mRightAnchorContainer.getVisibility());
    }

    @Test
    public void testRightAnchorContainerVisibility() {
        String unexpectedLeft = "Unexpected left container visibility.";
        String unexpectedRight = "Unexpected right container visibility.";
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.RIGHT);
        mCoordinator.registerSideUiContainer(sideUiContainer);

        // Verify starting visibility.
        assertEquals(unexpectedLeft, View.GONE, mLeftAnchorContainer.getVisibility());
        assertEquals(unexpectedRight, View.GONE, mRightAnchorContainer.getVisibility());

        // Start at RIGHT.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(
                        sideUiContainer.getSideUiId(),
                        sideUiContainer.getAnchorSide(),
                        TEST_SIDE_UI_WIDTH),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(unexpectedLeft, View.GONE, mLeftAnchorContainer.getVisibility());
        assertEquals(unexpectedRight, View.VISIBLE, mRightAnchorContainer.getVisibility());

        // Detach.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(
                        sideUiContainer.getSideUiId(),
                        sideUiContainer.getAnchorSide(),
                        /* width= */ 0),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(unexpectedLeft, View.GONE, mLeftAnchorContainer.getVisibility());
        assertEquals(unexpectedRight, View.GONE, mRightAnchorContainer.getVisibility());
    }

    @Test
    public void testOnTopMarginChanged() {
        // Set initial params, since these Views aren't actually attached.
        mLeftAnchorContainer.setLayoutParams(new MarginLayoutParams(0, 0));
        mRightAnchorContainer.setLayoutParams(new MarginLayoutParams(0, 0));

        // Notify of a top margin change.
        @Px int topMarginPx = 30;
        mTopMarginSupplier.set(topMarginPx);

        // Verify the topMargin is set appropriately.
        MarginLayoutParams leftLayoutParams =
                ((MarginLayoutParams) mLeftAnchorContainer.getLayoutParams());
        assertEquals("Unexpected top margin.", topMarginPx, leftLayoutParams.topMargin);

        MarginLayoutParams rightLayoutParams =
                ((MarginLayoutParams) mRightAnchorContainer.getLayoutParams());
        assertEquals("Unexpected top margin.", topMarginPx, rightLayoutParams.topMargin);
    }

    @Test
    public void
            testOnConfigurationChanged_WindowBecomesTooNarrowThenWideEnough_CloseAndReopenSideUi() {
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.RIGHT);
        mCoordinator.registerSideUiContainer(sideUiContainer);
        sideUiContainer.mMinWidthDp = 200;

        // Open a side UI.
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(
                        sideUiContainer.getSideUiId(),
                        sideUiContainer.getAnchorSide(),
                        TEST_SIDE_UI_WIDTH),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        // Simulate a configuration change that the window becomes too narrow.
        // The new configuration should force TestSideUiContainer#determineContainerWidth() to
        // return 0.
        int minWindowWidthDpForVisibleSideUi =
                MIN_WEB_CONTENTS_WIDTH_DP + sideUiContainer.mMinWidthDp;
        RuntimeEnvironment.setQualifiers(
                "w" + (minWindowWidthDpForVisibleSideUi - 1) + "dp-h1080dp-mdpi");
        mCoordinator.onConfigurationChanged(new Configuration());
        RobolectricUtil.runAllBackgroundAndUi();

        // SideUiContainer should be notified to close itself.
        assertEquals(0, getSideUiContainerViewWidth());

        // Simulate another configuration change that the window becomes wide enough again.
        // The new configuration should make TestSideUiContainer#determineContainerWidth() return
        // a positive value.
        RuntimeEnvironment.setQualifiers(
                "w" + minWindowWidthDpForVisibleSideUi + "dp-h1080dp-mdpi");
        mCoordinator.onConfigurationChanged(new Configuration());
        RobolectricUtil.runAllBackgroundAndUi();

        // SideUiContainer should be re-opened.
        assertNotEquals(0, getSideUiContainerViewWidth());
    }

    @Test
    public void testOnConfigurationChanged_SideUiCanStayOpen_SideUiSpecsChanged_ApplyNewSpecs() {
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.RIGHT);
        sideUiContainer.mMinWidthDp = 200;
        mCoordinator.registerSideUiContainer(sideUiContainer);
        mCoordinator.addObserver(mSideUiObserver);

        // Open a side UI.
        // Note the initial side UI width is larger than the minimum width.
        @Px int initialSideUiWidth = sideUiContainer.mMinWidthDp + 100;
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(
                        sideUiContainer.getSideUiId(),
                        sideUiContainer.getAnchorSide(),
                        initialSideUiWidth),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        // Simulate a configuration change.
        // The new configuration should force the side UI to have the minimum width, but it can stay
        // open.
        clearInvocations(mSideUiObserver);
        int minWindowWidthDpForVisibleSideUi =
                MIN_WEB_CONTENTS_WIDTH_DP + sideUiContainer.mMinWidthDp;
        RuntimeEnvironment.setQualifiers(
                "w" + minWindowWidthDpForVisibleSideUi + "dp-h1080dp-mdpi");
        mCoordinator.onConfigurationChanged(new Configuration());
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify that observers are notified with the updated specs.
        Map<Integer, Integer> sideUiWidths = new ArrayMap<>();
        sideUiWidths.put(AnchorSide.LEFT, 0);
        sideUiWidths.put(AnchorSide.RIGHT, sideUiContainer.mMinWidthDp);
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(sideUiWidths);
        verify(mSideUiObserver).onSideUiSpecsChanged(eq(expectedSideUiSpecs));

        // Verify the container view's width is updated.
        assertEquals(sideUiContainer.mMinWidthDp, getSideUiContainerViewWidth());
    }

    @Test
    public void testOnConfigurationChanged_SideUiCanStayOpen_SideUiSpecsNotChanged_NoOp() {
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.RIGHT);
        mCoordinator.registerSideUiContainer(sideUiContainer);
        mCoordinator.addObserver(mSideUiObserver);

        // Open a side UI.
        @Px int initialSideUiWidth = TEST_SIDE_UI_WIDTH;
        mCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(
                        sideUiContainer.getSideUiId(),
                        sideUiContainer.getAnchorSide(),
                        initialSideUiWidth),
                /* suppressAnimations= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        // Simulate a configuration change.
        // The new configuration should still have enough width for the initial side UI width.
        clearInvocations(mSideUiObserver);
        @Px int minWebContentsWidth = ViewUtils.dpToPx(mTestActivity, MIN_WEB_CONTENTS_WIDTH_DP);
        RuntimeEnvironment.setQualifiers(
                "w" + (minWebContentsWidth + initialSideUiWidth) + "dp-h1080dp-mdpi");
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
