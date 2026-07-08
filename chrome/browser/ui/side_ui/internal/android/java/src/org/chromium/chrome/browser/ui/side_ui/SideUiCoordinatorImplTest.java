// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.MIN_WEB_CONTENTS_WIDTH_DP;

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
import org.mockito.ArgumentCaptor;
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
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiShowability;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.ViewUtils;

import java.util.List;
import java.util.Map;

/** Unit tests for {@link SideUiCoordinatorImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "w1920dp-h1080dp-mdpi" /* windowWidth = 1920dp; 1920dp = 1920px (mdpi) */)
public class SideUiCoordinatorImplTest {

    /** Window size in this test; it must match {@code @Config}. */
    private static final Size WINDOW_SIZE_PX = new Size(1920, 1080);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private ViewStub mLeftAnchorContainerStub;
    @Mock private ViewStub mRightAnchorContainerStub;
    @Mock private ViewStub mWebContentHairlineContainerStub;
    @Mock private SideUiObserver mSideUiObserver;

    private final SettableNonNullObservableSupplier<Integer> mTopMarginSupplier =
            ObservableSuppliers.createNonNull(0);

    private Activity mTestActivity;
    private ViewGroup mLeftAnchorContainer;
    private ViewGroup mRightAnchorContainer;
    private View mSideUiContainerView;
    private SideUiCoordinatorImpl mCoordinator;

    @Before
    public void setUp() {
        mTestActivity = Robolectric.buildActivity(TestActivity.class).setup().get();

        // Set up the parent View of side UI anchor containers.
        FrameLayout anchorContainerParent = new FrameLayout(mTestActivity);
        mTestActivity.addContentView(
                anchorContainerParent,
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
        anchorContainerParent.addView(
                mLeftAnchorContainer,
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT));
        anchorContainerParent.addView(
                mRightAnchorContainer,
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT));

        doReturn(mLeftAnchorContainer).when(mLeftAnchorContainerStub).inflate();
        doReturn(mRightAnchorContainer).when(mRightAnchorContainerStub).inflate();

        SideUiWebContentHairlineContainer webContentHairlineContainer =
                (SideUiWebContentHairlineContainer)
                        mTestActivity
                                .getLayoutInflater()
                                .inflate(R.layout.side_ui_web_content_hairline_container, null);
        webContentHairlineContainer.setLayoutParams(new MarginLayoutParams(0, 0));
        doReturn(webContentHairlineContainer).when(mWebContentHairlineContainerStub).inflate();

        // Initialize the SideUiCoordinator under test.
        mCoordinator =
                new SideUiCoordinatorImpl(
                        mTestActivity,
                        mActivityLifecycleDispatcher,
                        mBrowserControlsStateProvider,
                        anchorContainerParent,
                        mLeftAnchorContainerStub,
                        mRightAnchorContainerStub,
                        mWebContentHairlineContainerStub,
                        mTopMarginSupplier);

        // Initialize the SideUiContainer View.
        mSideUiContainerView = new View(mTestActivity);

        // Make sure the measure pass and the layout pass are completed before running tests.
        RobolectricUtil.runAllBackgroundAndUi();

        // mAnchorContainerParent should have the size specified in @Config.
        assertEquals(WINDOW_SIZE_PX.getWidth(), anchorContainerParent.getWidth());
        assertEquals(WINDOW_SIZE_PX.getHeight(), anchorContainerParent.getHeight());
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
                mCoordinator.getSideUiContainerById(sideUiContainer.getSideUiId()),
                sideUiContainer);

        mCoordinator.unregisterSideUiContainer(sideUiContainer);
        assertNull(
                "Registered SideUiContainer expected to be null.",
                mCoordinator.getSideUiContainerById(sideUiContainer.getSideUiId()));
    }

    @Test
    public void testUpdateUi_AnchorSideIsLeft() {
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.LEFT);
        mCoordinator.registerSideUiContainer(sideUiContainer);
        mCoordinator.addObserver(mSideUiObserver);
        clearInvocations(mSideUiObserver);

        mCoordinator.updateUi(
                new UiUpdateRequest(sideUiContainer.getSideUiId(), /* suppressAnimations= */ true));

        // Verify observers notified.
        @Px
        int expectedLeftSideUiWidth = ViewUtils.dpToPx(mTestActivity, sideUiContainer.mMaxWidthDp);
        Map<@AnchorSide Integer, Integer> sideUiWidths = new ArrayMap<>();
        sideUiWidths.put(AnchorSide.LEFT, expectedLeftSideUiWidth);
        sideUiWidths.put(AnchorSide.RIGHT, 0);
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(sideUiWidths);
        verify(mSideUiObserver).onSideUiSpecsChanged(eq(expectedSideUiSpecs));

        // Verify view attached to left container.
        assertEquals(mLeftAnchorContainer, mSideUiContainerView.getParent());
        assertEquals(expectedLeftSideUiWidth, mSideUiContainerView.getWidth());
    }

    @Test
    public void testUpdateUi_AnchorSideIsRight() {
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.RIGHT);
        mCoordinator.registerSideUiContainer(sideUiContainer);
        mCoordinator.addObserver(mSideUiObserver);
        clearInvocations(mSideUiObserver);

        mCoordinator.updateUi(
                new UiUpdateRequest(sideUiContainer.getSideUiId(), /* suppressAnimations= */ true));

        // Verify observers notified.
        @Px
        int expectedRightSideUiWidth = ViewUtils.dpToPx(mTestActivity, sideUiContainer.mMaxWidthDp);
        Map<@AnchorSide Integer, Integer> sideUiWidths = new ArrayMap<>();
        sideUiWidths.put(AnchorSide.LEFT, 0);
        sideUiWidths.put(AnchorSide.RIGHT, expectedRightSideUiWidth);
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(sideUiWidths);
        verify(mSideUiObserver).onSideUiSpecsChanged(eq(expectedSideUiSpecs));

        // Verify view attached to right container.
        assertEquals(mRightAnchorContainer, mSideUiContainerView.getParent());
        assertEquals(expectedRightSideUiWidth, mSideUiContainerView.getWidth());
    }

    @Test
    public void testUpdateUi_twoSideUiContainers() {
        int windowWidthDp = ViewUtils.pxToDp(mTestActivity, WINDOW_SIZE_PX.getWidth());

        // Arrange: Register the right SideUiContainer.
        // Note that the container's max & min widths ensure the window will only have enough space
        // for this container.
        View rightUiContainerView = new FrameLayout(mTestActivity);
        var rightUiContainer =
                new TestSideUiContainer(
                        mCoordinator,
                        rightUiContainerView,
                        SideUiId.SIDE_UI_FOR_TESTING_LOW_PRIORITY,
                        AnchorSide.RIGHT);
        rightUiContainer.mMinWidthDp = windowWidthDp - SideUiCoordinator.MIN_WEB_CONTENTS_WIDTH_DP;
        rightUiContainer.mMaxWidthDp = rightUiContainer.mMinWidthDp;
        @Px
        int expectedRightSideUiWidth =
                ViewUtils.dpToPx(mTestActivity, rightUiContainer.mMaxWidthDp);
        mCoordinator.registerSideUiContainer(rightUiContainer);

        // Arrange: Register the left SideUiContainer.
        // Note that the container's max & min widths ensure the window will only have enough space
        // for this container.
        View leftUiContainerView = new FrameLayout(mTestActivity);
        var leftUiContainer =
                new TestSideUiContainer(
                        mCoordinator,
                        leftUiContainerView,
                        SideUiId.SIDE_UI_FOR_TESTING_HIGH_PRIORITY,
                        AnchorSide.LEFT);
        leftUiContainer.mMinWidthDp = windowWidthDp - SideUiCoordinator.MIN_WEB_CONTENTS_WIDTH_DP;
        leftUiContainer.mMaxWidthDp = leftUiContainer.mMinWidthDp;
        @Px
        int expectedLeftSideUiWidth = ViewUtils.dpToPx(mTestActivity, leftUiContainer.mMaxWidthDp);
        mCoordinator.registerSideUiContainer(leftUiContainer);

        // Arrange: Add an observer.
        mCoordinator.addObserver(mSideUiObserver);

        // Act: Show only the right SideUiContainer.
        rightUiContainer.mHasContentToShow = true;
        leftUiContainer.mHasContentToShow = false;
        clearInvocations(mSideUiObserver);
        mCoordinator.updateUi(
                new UiUpdateRequest(
                        rightUiContainer.getSideUiId(), /* suppressAnimations= */ true));

        // Assert: The right SideUiContainer is shown.
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(0, expectedRightSideUiWidth);
        SideUiSpecs currentSideUiSpecs = mCoordinator.getCurrentSideUiSpecs();
        assertEquals(expectedSideUiSpecs, currentSideUiSpecs);
        assertEquals(expectedRightSideUiWidth, rightUiContainerView.getLayoutParams().width);

        // Assert: Neither container will receive auto-close/auto-restore notifications.
        assertEquals(0, rightUiContainer.mNumOnWillAutoCloseReceived);
        assertEquals(0, rightUiContainer.mNumOnWillAutoRestoreReceived);
        assertEquals(0, leftUiContainer.mNumOnWillAutoCloseReceived);
        assertEquals(0, leftUiContainer.mNumOnWillAutoRestoreReceived);

        // Assert: Only the right container should receive onUiUpdateCompleted notification.
        assertEquals(1, rightUiContainer.mNumOnUiUpdateCompletedReceived);
        assertEquals(Integer.valueOf(0), rightUiContainer.mLastOldWidth);
        assertEquals(Integer.valueOf(expectedRightSideUiWidth), rightUiContainer.mLastNewWidth);
        assertEquals(0, leftUiContainer.mNumOnUiUpdateCompletedReceived);

        // Assert: The observer is notified with both containers being showable.
        //
        // The right SideUiContainer is currently visible, so it's definitely showable.
        // The left SideUiContainer isn't visible, but it has higher priority. If it needs to be
        // shown, it will force the right SideUiContainer to be hidden. So the left SideUiContainer
        // is also showable.
        ArgumentCaptor<SideUiShowability> showabilityCaptor =
                ArgumentCaptor.forClass(SideUiShowability.class);
        verify(mSideUiObserver).onShowableSideUisUpdated(showabilityCaptor.capture());
        assertEquals(
                List.of(
                        SideUiId.SIDE_UI_FOR_TESTING_HIGH_PRIORITY,
                        SideUiId.SIDE_UI_FOR_TESTING_LOW_PRIORITY),
                showabilityCaptor.getValue().mShowableSideUiIds);
        assertTrue(showabilityCaptor.getValue().mUnshowableSideUiIds.isEmpty());

        // Act: Attempt to show both SideUiContainers.
        rightUiContainer.mHasContentToShow = true;
        leftUiContainer.mHasContentToShow = true;
        clearInvocations(mSideUiObserver);
        mCoordinator.updateUi(
                new UiUpdateRequest(leftUiContainer.getSideUiId(), /* suppressAnimations= */ true));

        // Assert: The left SideUiContainer is shown, but the right container is hidden.
        expectedSideUiSpecs = new SideUiSpecs(expectedLeftSideUiWidth, 0);
        currentSideUiSpecs = mCoordinator.getCurrentSideUiSpecs();
        assertEquals(expectedSideUiSpecs, currentSideUiSpecs);
        assertEquals(expectedLeftSideUiWidth, leftUiContainerView.getLayoutParams().width);
        assertEquals(0, rightUiContainerView.getLayoutParams().width);

        // Assert: The right SideUiContainer should receive the auto-close notification.
        assertEquals(1, rightUiContainer.mNumOnWillAutoCloseReceived);
        assertEquals(0, rightUiContainer.mNumOnWillAutoRestoreReceived);
        assertEquals(0, leftUiContainer.mNumOnWillAutoCloseReceived);
        assertEquals(0, leftUiContainer.mNumOnWillAutoRestoreReceived);

        // Assert: Both containers should receive onUiUpdateCompleted notification.
        assertEquals(2, rightUiContainer.mNumOnUiUpdateCompletedReceived);
        assertEquals(Integer.valueOf(expectedRightSideUiWidth), rightUiContainer.mLastOldWidth);
        assertEquals(Integer.valueOf(0), rightUiContainer.mLastNewWidth);
        assertEquals(1, leftUiContainer.mNumOnUiUpdateCompletedReceived);
        assertEquals(Integer.valueOf(0), leftUiContainer.mLastOldWidth);
        assertEquals(Integer.valueOf(expectedLeftSideUiWidth), leftUiContainer.mLastNewWidth);

        // Assert: The observer is notified that the right (low-priority) container is no longer
        // showable.
        verify(mSideUiObserver).onShowableSideUisUpdated(showabilityCaptor.capture());
        assertEquals(
                List.of(SideUiId.SIDE_UI_FOR_TESTING_HIGH_PRIORITY),
                showabilityCaptor.getValue().mShowableSideUiIds);
        assertEquals(
                List.of(SideUiId.SIDE_UI_FOR_TESTING_LOW_PRIORITY),
                showabilityCaptor.getValue().mUnshowableSideUiIds);

        // Act: Close the left container.
        leftUiContainer.mHasContentToShow = false;
        clearInvocations(mSideUiObserver);
        mCoordinator.updateUi(
                new UiUpdateRequest(leftUiContainer.getSideUiId(), /* suppressAnimations= */ true));

        // Assert: The left SideUiContainer is hidden, and the right container is auto-restored.
        expectedSideUiSpecs = new SideUiSpecs(0, expectedRightSideUiWidth);
        currentSideUiSpecs = mCoordinator.getCurrentSideUiSpecs();
        assertEquals(expectedSideUiSpecs, currentSideUiSpecs);
        assertEquals(0, leftUiContainerView.getLayoutParams().width);
        assertEquals(expectedRightSideUiWidth, rightUiContainerView.getLayoutParams().width);

        // Assert: The right SideUiContainer should receive the auto-restore notification.
        assertEquals(1, rightUiContainer.mNumOnWillAutoCloseReceived);
        assertEquals(1, rightUiContainer.mNumOnWillAutoRestoreReceived);
        assertEquals(0, leftUiContainer.mNumOnWillAutoCloseReceived);
        assertEquals(0, leftUiContainer.mNumOnWillAutoRestoreReceived);

        // Assert: Both containers should receive onUiUpdateCompleted notification.
        assertEquals(3, rightUiContainer.mNumOnUiUpdateCompletedReceived);
        assertEquals(Integer.valueOf(0), rightUiContainer.mLastOldWidth);
        assertEquals(Integer.valueOf(expectedRightSideUiWidth), rightUiContainer.mLastNewWidth);
        assertEquals(2, leftUiContainer.mNumOnUiUpdateCompletedReceived);
        assertEquals(Integer.valueOf(expectedLeftSideUiWidth), leftUiContainer.mLastOldWidth);
        assertEquals(Integer.valueOf(0), leftUiContainer.mLastNewWidth);

        // Assert: The observer is notified that both containers are showable.
        verify(mSideUiObserver).onShowableSideUisUpdated(showabilityCaptor.capture());
        assertEquals(
                List.of(
                        SideUiId.SIDE_UI_FOR_TESTING_HIGH_PRIORITY,
                        SideUiId.SIDE_UI_FOR_TESTING_LOW_PRIORITY),
                showabilityCaptor.getValue().mShowableSideUiIds);
        assertTrue(showabilityCaptor.getValue().mUnshowableSideUiIds.isEmpty());
    }

    @Test
    public void testUpdateUi_preventsReentrancy() {
        int windowWidthDp = ViewUtils.pxToDp(mTestActivity, WINDOW_SIZE_PX.getWidth());

        // Arrange: Register the right SideUiContainer.
        // Note that the container's max & min widths ensure the window will only have enough space
        // for this container.
        View rightUiContainerView = new FrameLayout(mTestActivity);
        var rightUiContainer =
                new TestSideUiContainer(
                        mCoordinator,
                        rightUiContainerView,
                        SideUiId.SIDE_UI_FOR_TESTING_LOW_PRIORITY,
                        AnchorSide.RIGHT);
        rightUiContainer.mMinWidthDp = windowWidthDp - SideUiCoordinator.MIN_WEB_CONTENTS_WIDTH_DP;
        rightUiContainer.mMaxWidthDp = rightUiContainer.mMinWidthDp;
        @Px
        int expectedRightSideUiWidth =
                ViewUtils.dpToPx(mTestActivity, rightUiContainer.mMaxWidthDp);
        mCoordinator.registerSideUiContainer(rightUiContainer);

        // Arrange: Register the left SideUiContainer.
        // Note that the container's max & min widths ensure the window will only have enough space
        // for this container.
        View leftUiContainerView = new FrameLayout(mTestActivity);
        var leftUiContainer =
                new TestSideUiContainer(
                        mCoordinator,
                        leftUiContainerView,
                        SideUiId.SIDE_UI_FOR_TESTING_HIGH_PRIORITY,
                        AnchorSide.LEFT);
        leftUiContainer.mMinWidthDp = windowWidthDp - SideUiCoordinator.MIN_WEB_CONTENTS_WIDTH_DP;
        leftUiContainer.mMaxWidthDp = leftUiContainer.mMinWidthDp;
        mCoordinator.registerSideUiContainer(leftUiContainer);

        // Act: Show only the right SideUiContainer.
        rightUiContainer.mHasContentToShow = true;
        leftUiContainer.mHasContentToShow = false;
        mCoordinator.updateUi(
                new UiUpdateRequest(
                        rightUiContainer.getSideUiId(), /* suppressAnimations= */ true));

        // Assert: The right SideUiContainer is shown.
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(0, expectedRightSideUiWidth);
        SideUiSpecs currentSideUiSpecs = mCoordinator.getCurrentSideUiSpecs();
        assertEquals(expectedSideUiSpecs, currentSideUiSpecs);

        // Act & Assert: Attempt to show both SideUiContainers.
        // This will cause the right SideUiContainer to auto-close.
        // The right SideUiContainer is configured to call updateUi() in onWillAutoClose(), which
        // will cause re-entrancy into updateUi().
        rightUiContainer.mRequestUiUpdateOnWillAutoClose = true;
        rightUiContainer.mHasContentToShow = true;
        leftUiContainer.mHasContentToShow = true;
        var request =
                new UiUpdateRequest(leftUiContainer.getSideUiId(), /* suppressAnimations= */ true);
        assertThrows(AssertionError.class, () -> mCoordinator.updateUi(request));
    }

    @Test
    public void testUpdateUi_DetachOnClose() {
        // Arrange: Register a SideUiContainer.
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.RIGHT);
        mCoordinator.registerSideUiContainer(sideUiContainer);

        // Arrange: Attach the SideUiContainer View.
        UiUpdateRequest sideUiProperties =
                new UiUpdateRequest(sideUiContainer.getSideUiId(), /* suppressAnimations= */ true);
        mCoordinator.updateUi(sideUiProperties);
        assertEquals(mRightAnchorContainer, mSideUiContainerView.getParent());

        // Act: Close the SideUiContainer.
        sideUiContainer.mHasContentToShow = false;
        mCoordinator.updateUi(sideUiProperties);

        // Assert: The SideUiContainer View is detached.
        // Note that the View's getWidth() will now return a stale width (non-zero) since it's
        // detached, but its LayoutParams.width should be 0.
        assertNull(mSideUiContainerView.getParent());
        assertEquals(0, mSideUiContainerView.getLayoutParams().width);
    }

    @Test
    public void testUpdateUi_InvokeDetermineShowableWidth() {
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.RIGHT);
        mCoordinator.registerSideUiContainer(sideUiContainer);

        mCoordinator.updateUi(
                new UiUpdateRequest(sideUiContainer.getSideUiId(), /* suppressAnimations= */ true));

        // Verify SideUiContainer#determineShowableWidth() is invoked with correct parameters.
        int minWebContentsWidthPx = ViewUtils.dpToPx(mTestActivity, MIN_WEB_CONTENTS_WIDTH_DP);
        assertEquals(
                Integer.valueOf(WINDOW_SIZE_PX.getWidth() - minWebContentsWidthPx),
                sideUiContainer.mLastAvailableWidth);
        assertEquals(Integer.valueOf(WINDOW_SIZE_PX.getWidth()), sideUiContainer.mLastWindowWidth);
    }

    @Test
    public void testUpdateUi_noUiChange_onUiUpdateCompletedNotCalled() {
        // Arrange: Register and show a SideUiContainer.
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.RIGHT);
        mCoordinator.registerSideUiContainer(sideUiContainer);
        mCoordinator.updateUi(
                new UiUpdateRequest(sideUiContainer.getSideUiId(), /* suppressAnimations= */ true));

        // Assert:
        @Px int sideUiWidth = mSideUiContainerView.getWidth();
        assertEquals(1, sideUiContainer.mNumOnUiUpdateCompletedReceived);
        assertEquals(Integer.valueOf(0), sideUiContainer.mLastOldWidth);
        assertEquals(Integer.valueOf(sideUiWidth), sideUiContainer.mLastNewWidth);

        // Act: Trigger another UI update. This update should be a no-op.
        mCoordinator.updateUi(
                new UiUpdateRequest(sideUiContainer.getSideUiId(), /* suppressAnimations= */ true));

        // Assert: onUiUpdateCompleted shouldn't be called again.
        assertEquals(1, sideUiContainer.mNumOnUiUpdateCompletedReceived);
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
        var sideUiProperties =
                new UiUpdateRequest(sideUiContainer.getSideUiId(), /* suppressAnimations= */ true);
        mCoordinator.updateUi(sideUiProperties);
        assertEquals(unexpectedLeft, View.VISIBLE, mLeftAnchorContainer.getVisibility());
        assertEquals(unexpectedRight, View.GONE, mRightAnchorContainer.getVisibility());

        // Detach.
        sideUiContainer.mHasContentToShow = false;
        mCoordinator.updateUi(sideUiProperties);
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
        var sideUiProperties =
                new UiUpdateRequest(sideUiContainer.getSideUiId(), /* suppressAnimations= */ true);
        mCoordinator.updateUi(sideUiProperties);
        assertEquals(unexpectedLeft, View.GONE, mLeftAnchorContainer.getVisibility());
        assertEquals(unexpectedRight, View.VISIBLE, mRightAnchorContainer.getVisibility());

        // Detach.
        sideUiContainer.mHasContentToShow = false;
        mCoordinator.updateUi(sideUiProperties);
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
        mCoordinator.updateUi(
                new UiUpdateRequest(sideUiContainer.getSideUiId(), /* suppressAnimations= */ true));

        // Simulate a configuration change that the window becomes too narrow.
        // The new configuration should force TestSideUiContainer#determineShowableWidth() to
        // return 0.
        int minWindowWidthDpForVisibleSideUi =
                MIN_WEB_CONTENTS_WIDTH_DP + sideUiContainer.mMinWidthDp;
        RuntimeEnvironment.setQualifiers(
                "w" + (minWindowWidthDpForVisibleSideUi - 1) + "dp-h1080dp-mdpi");
        mCoordinator.onConfigurationChanged(new Configuration());

        // SideUiContainer should be notified to close itself (detached from its parent).
        assertNull(mSideUiContainerView.getParent());

        // Simulate another configuration change that the window becomes wide enough again.
        // The new configuration should make TestSideUiContainer#determineShowableWidth() return
        // a positive value.
        RuntimeEnvironment.setQualifiers(
                "w" + minWindowWidthDpForVisibleSideUi + "dp-h1080dp-mdpi");
        mCoordinator.onConfigurationChanged(new Configuration());

        // SideUiContainer should be re-opened.
        assertNotEquals(0, mSideUiContainerView.getWidth());
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
        mCoordinator.updateUi(
                new UiUpdateRequest(sideUiContainer.getSideUiId(), /* suppressAnimations= */ true));

        // Simulate a configuration change.
        // The new configuration should force the side UI to have the minimum width, but it can stay
        // open.
        clearInvocations(mSideUiObserver);
        int minWindowWidthDpForVisibleSideUi =
                MIN_WEB_CONTENTS_WIDTH_DP + sideUiContainer.mMinWidthDp;
        RuntimeEnvironment.setQualifiers(
                "w" + minWindowWidthDpForVisibleSideUi + "dp-h1080dp-mdpi");
        mCoordinator.onConfigurationChanged(new Configuration());

        // Verify that observers are notified with the updated specs.
        @Px
        int expectedRightSideUiWidth = ViewUtils.dpToPx(mTestActivity, sideUiContainer.mMinWidthDp);
        Map<@AnchorSide Integer, Integer> sideUiWidths = new ArrayMap<>();
        sideUiWidths.put(AnchorSide.LEFT, 0);
        sideUiWidths.put(AnchorSide.RIGHT, expectedRightSideUiWidth);
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(sideUiWidths);
        verify(mSideUiObserver).onSideUiSpecsChanged(eq(expectedSideUiSpecs));

        // Verify the container view's width is updated.
        assertEquals(expectedRightSideUiWidth, mSideUiContainerView.getWidth());
    }

    @Test
    public void testOnConfigurationChanged_SideUiCanStayOpen_SideUiSpecsNotChanged_NoOp() {
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.RIGHT);
        mCoordinator.registerSideUiContainer(sideUiContainer);
        mCoordinator.addObserver(mSideUiObserver);

        // Open a side UI.
        mCoordinator.updateUi(
                new UiUpdateRequest(sideUiContainer.getSideUiId(), /* suppressAnimations= */ true));
        @Px int sideUiWidth = mSideUiContainerView.getWidth();

        // Simulate a configuration change.
        // The new configuration should still have enough width for the initial side UI width.
        clearInvocations(mSideUiObserver);
        int newWindowWidthDp = MIN_WEB_CONTENTS_WIDTH_DP + sideUiContainer.mMaxWidthDp;
        RuntimeEnvironment.setQualifiers("w" + newWindowWidthDp + "dp-h1080dp-mdpi");
        mCoordinator.onConfigurationChanged(new Configuration());

        // Verify that the observer is NOT notified of the showable state or the specs since neither
        // was changed.
        verify(mSideUiObserver, never()).onShowableSideUisUpdated(any());
        verify(mSideUiObserver, never()).onSideUiSpecsChanged(any());

        // Verify the container view's width is unchanged.
        assertEquals(sideUiWidth, mSideUiContainerView.getWidth());
    }

    @Test
    public void testCanShowSideUi() {
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.RIGHT);
        sideUiContainer.mMinWidthDp = 200;
        mCoordinator.registerSideUiContainer(sideUiContainer);

        // 1. Initially, window is wide (1920dp). Should be able to show SideUi.
        assertTrue(mCoordinator.canShowSideUi(sideUiContainer.getSideUiId()));

        // 2. Shrink window below threshold: minWebContentsWidth (412) + minSidePanelWidth (200) =
        // 612dp.
        int minWindowWidthDpForVisibleSideUi =
                MIN_WEB_CONTENTS_WIDTH_DP + sideUiContainer.mMinWidthDp;
        RuntimeEnvironment.setQualifiers(
                "w" + (minWindowWidthDpForVisibleSideUi - 1) + "dp-h1080dp-mdpi");
        mCoordinator.onConfigurationChanged(new Configuration());

        // Should not be able to show side UI.
        assertFalse(mCoordinator.canShowSideUi(sideUiContainer.getSideUiId()));
    }

    @Test
    public void testOnConfigurationChanged_ShowableContainerIdsChange_NotifyObservers() {
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.RIGHT);
        sideUiContainer.mMinWidthDp = 200;
        mCoordinator.registerSideUiContainer(sideUiContainer);
        mCoordinator.addObserver(mSideUiObserver);
        clearInvocations(mSideUiObserver);

        // 1. Shrink window so side UI cannot be shown.
        int minWindowWidthDpForVisibleSideUi =
                MIN_WEB_CONTENTS_WIDTH_DP + sideUiContainer.mMinWidthDp;
        RuntimeEnvironment.setQualifiers(
                "w" + (minWindowWidthDpForVisibleSideUi - 1) + "dp-h1080dp-mdpi");
        mCoordinator.onConfigurationChanged(new Configuration());

        // Verify the observer is notified that the container can no longer be shown.
        ArgumentCaptor<SideUiShowability> showabilityCaptor =
                ArgumentCaptor.forClass(SideUiShowability.class);
        verify(mSideUiObserver).onShowableSideUisUpdated(showabilityCaptor.capture());
        assertTrue(showabilityCaptor.getValue().mShowableSideUiIds.isEmpty());
        assertEquals(
                List.of(SideUiId.SIDE_PANEL), showabilityCaptor.getValue().mUnshowableSideUiIds);

        clearInvocations(mSideUiObserver);

        // 2. Grow window back to wide.
        RuntimeEnvironment.setQualifiers(
                "w" + minWindowWidthDpForVisibleSideUi + "dp-h1080dp-mdpi");
        mCoordinator.onConfigurationChanged(new Configuration());

        // Verify the observer is notified that the container can be shown again.
        verify(mSideUiObserver).onShowableSideUisUpdated(showabilityCaptor.capture());
        assertEquals(List.of(SideUiId.SIDE_PANEL), showabilityCaptor.getValue().mShowableSideUiIds);
        assertTrue(showabilityCaptor.getValue().mUnshowableSideUiIds.isEmpty());
    }

    @Test
    public void testEndAnimations_BeforeTransitionStarts_EndTransitionAndNotifyObservers() {
        // Arrange:
        var sideUiContainer =
                new TestSideUiContainer(
                        mCoordinator, mSideUiContainerView, SideUiId.SIDE_PANEL, AnchorSide.RIGHT);
        mCoordinator.registerSideUiContainer(sideUiContainer);

        mCoordinator.addObserver(mSideUiObserver);
        clearInvocations(mSideUiObserver);

        mCoordinator.updateUi(
                new UiUpdateRequest(
                        sideUiContainer.getSideUiId(), /* suppressAnimations= */ false));

        // Act: Immediately call endAnimations() after updateUi().
        // The transition hasn't started at this point.
        mCoordinator.endAnimations();

        // Assert: SideUiContainer has the correct width and received onUiUpdateCompleted().
        @Px int expectedWidth = ViewUtils.dpToPx(mTestActivity, sideUiContainer.mMaxWidthDp);
        assertEquals(expectedWidth, mSideUiContainerView.getWidth());
        assertEquals(1, sideUiContainer.mNumOnUiUpdateCompletedReceived);
        assertEquals(Integer.valueOf(0), sideUiContainer.mLastOldWidth);
        assertEquals(Integer.valueOf(expectedWidth), sideUiContainer.mLastNewWidth);

        // Assert: SideUiObserver received onTransitionBegun() and onTransitionEnded().
        Map<@AnchorSide Integer, Integer> sideUiWidths = new ArrayMap<>();
        sideUiWidths.put(AnchorSide.LEFT, 0);
        sideUiWidths.put(AnchorSide.RIGHT, expectedWidth);
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(sideUiWidths);
        verify(mSideUiObserver).onTransitionBegun(expectedSideUiSpecs);
        verify(mSideUiObserver).onTransitionEnded(expectedSideUiSpecs);
    }
}
