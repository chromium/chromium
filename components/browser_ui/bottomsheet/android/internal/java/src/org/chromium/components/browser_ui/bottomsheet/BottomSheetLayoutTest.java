// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.when;
import static org.robolectric.Robolectric.buildActivity;

import static org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.MAX_HEIGHT_RATIO;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.MathUtils;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetView.SheetLayoutMode;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * End-to-end integration tests for {@link BottomSheetControllerImpl} and {@link
 * BottomSheetCoordinator} covering Z-axis and bottom-controls anchoring, large form factor layout
 * mode and caller margin transitions, {@link HeightMode#WRAP_CONTENT} reflow and invalidation,
 * animated {@link BottomSheetContent#coversBottomControls()} inset transitions, and dynamic {@link
 * HeightMode#RESIZE_CONTENT} split-screen transitions.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetLayoutTest {
    private static final int CONTAINER_WIDTH = 1080;
    private static final int CONTAINER_HEIGHT = 2000;

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock ScrimManager mScrimManager;
    private @Mock KeyboardVisibilityDelegate mKeyboardDelegate;
    private @Mock InsetObserver mInsetObserver;

    private Activity mActivity;
    private FrameLayout mOuterRoot;
    private FrameLayout mSheetContainer;
    private BottomSheetControllerImpl mController;
    private SettableNonNullObservableSupplier<Integer> mKeyboardInsetSupplier;
    private int mE2eBottomInsetDp;
    private final List<BottomSheetCoordinator> mCoordinatorsToDestroy = new ArrayList<>();

    @Before
    public void setUp() {
        mActivity = buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mOuterRoot = new FrameLayout(mActivity);
        mActivity.setContentView(mOuterRoot);
        mSheetContainer = new FrameLayout(mActivity);
        mOuterRoot.addView(
                mSheetContainer,
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        mKeyboardInsetSupplier = ObservableSuppliers.createNonNull(0);
        when(mInsetObserver.getSupplierForKeyboardInset()).thenReturn(mKeyboardInsetSupplier);

        lenient()
                .doAnswer(
                        inv -> {
                            PropertyModel params = inv.getArgument(0);
                            params.get(ScrimProperties.VISIBILITY_CALLBACK).onResult(true);
                            return null;
                        })
                .when(mScrimManager)
                .showScrim(any());

        lenient()
                .doAnswer(
                        inv -> {
                            PropertyModel params = inv.getArgument(0);
                            params.get(ScrimProperties.VISIBILITY_CALLBACK).onResult(false);
                            return null;
                        })
                .when(mScrimManager)
                .hideScrim(any(), anyBoolean());

        layoutOuterRoot(CONTAINER_WIDTH, CONTAINER_HEIGHT);

        OneshotSupplierImpl<ScrimManager> scrimSupplier = new OneshotSupplierImpl<>();
        scrimSupplier.set(mScrimManager);
        OneshotSupplierImpl<ViewGroup> rootSupplier = new OneshotSupplierImpl<>();
        rootSupplier.set(mSheetContainer);
        mController =
                new BottomSheetControllerImpl(
                        scrimSupplier,
                        mActivity.getWindow(),
                        mKeyboardDelegate,
                        rootSupplier,
                        /* alwaysFullWidth= */ false,
                        () -> mE2eBottomInsetDp,
                        /* desktopWindowStateManager= */ null,
                        mInsetObserver,
                        /* enableLargeFormFactorUi= */ false);
    }

    @After
    public void tearDown() {
        LocalizationUtils.setRtlForTesting(false);
        for (BottomSheetCoordinator coordinator : mCoordinatorsToDestroy) {
            coordinator.destroy();
        }
        mCoordinatorsToDestroy.clear();
        mController.destroy();
        mActivity.finish();
    }

    @Test
    public void testControllerAndCoordinator_ModalSheetZAxisAndBottomMarginLifecycle() {
        mE2eBottomInsetDp = 24;
        mController.runSheetInitializerForTesting();
        mController.setBottomControlsOffset(56);
        layoutOuterRoot(CONTAINER_WIDTH, CONTAINER_HEIGHT);

        // 1. No content: anchored above bottom controls with Z=0 and bottomMargin=56.
        assertTrue(mController.isAnchoredToBottomControls());
        assertEquals(0.0f, mSheetContainer.getZ(), MathUtils.EPSILON);
        assertEquals(56, mController.getContainerBottomMargin());
        assertEquals(56, ((MarginLayoutParams) mSheetContainer.getLayoutParams()).bottomMargin);
        assertEquals(CONTAINER_HEIGHT - 56, mController.getContainerHeight());
        assertEquals(CONTAINER_HEIGHT - 56, mController.getMaxSheetHeight());
        assertEquals(0, mController.getEdgeToEdgeBottomInset());

        // 2. Show modal content in PEEK: scrim is not yet visible, so sheet stays anchored at Z=0.
        TestSheetContent modalContent = new TestSheetContent(mActivity);
        modalContent.mPeekHeight = 120;
        modalContent.mHalfRatio = 0.5f;
        modalContent.mFullRatio = MAX_HEIGHT_RATIO;
        modalContent.mCoversBottomControls = false;
        modalContent.mHasCustomScrimLifecycle = false;

        assertTrue(mController.requestShowContent(modalContent, /* animate= */ false));
        assertEquals(SheetState.PEEK, mController.getSheetState());
        assertFalse(mController.isSheetOpen());
        assertTrue(mController.isAnchoredToBottomControls());
        assertEquals(0.0f, mSheetContainer.getZ(), MathUtils.EPSILON);
        assertEquals(56, mController.getContainerBottomMargin());
        assertEquals(120, mController.getCurrentOffset());
        assertEquals(0, mController.getEdgeToEdgeBottomInset());
        assertEquals(
                (CONTAINER_HEIGHT - 56) - 120f,
                mController.getBottomSheetForTesting().getView().getTranslationY(),
                MathUtils.EPSILON);

        // 3. Expand modal sheet to HALF: scrim becomes visible, lifting sheet to Z=1 and margin=0.
        mController.expandSheet(/* animate= */ false);
        layoutOuterRoot(CONTAINER_WIDTH, CONTAINER_HEIGHT);

        assertEquals(SheetState.HALF, mController.getSheetState());
        assertTrue(mController.isSheetOpen());
        assertFalse(mController.isAnchoredToBottomControls());
        assertEquals(1.0f, mSheetContainer.getZ(), MathUtils.EPSILON);
        assertEquals(0, mController.getContainerBottomMargin());
        assertEquals(0, ((MarginLayoutParams) mSheetContainer.getLayoutParams()).bottomMargin);
        assertEquals(CONTAINER_HEIGHT, mController.getContainerHeight());
        assertEquals(CONTAINER_HEIGHT, mController.getMaxSheetHeight());
        assertEquals(24, mController.getEdgeToEdgeBottomInset());
        assertEquals(1000, mController.getCurrentOffset());

        // 4. Updating bottomControlsOffset while modal sheet is open keeps Z=1 and margin=0.
        mController.setBottomControlsOffset(80);
        assertFalse(mController.isAnchoredToBottomControls());
        assertEquals(1.0f, mSheetContainer.getZ(), MathUtils.EPSILON);
        assertEquals(0, mController.getContainerBottomMargin());

        // 5. Collapse modal sheet back to PEEK: scrim hides, restoring Z=0 and margin=80.
        assertTrue(mController.collapseSheet(/* animate= */ false));
        layoutOuterRoot(CONTAINER_WIDTH, CONTAINER_HEIGHT);

        assertEquals(SheetState.PEEK, mController.getSheetState());
        assertFalse(mController.isSheetOpen());
        assertTrue(mController.isAnchoredToBottomControls());
        assertEquals(0.0f, mSheetContainer.getZ(), MathUtils.EPSILON);
        assertEquals(80, mController.getContainerBottomMargin());
        assertEquals(80, ((MarginLayoutParams) mSheetContainer.getLayoutParams()).bottomMargin);
        assertEquals(CONTAINER_HEIGHT - 80, mController.getContainerHeight());
        assertEquals(0, mController.getEdgeToEdgeBottomInset());

        // 6. Updating bottomControlsOffset while in PEEK immediately updates container margin.
        mController.setBottomControlsOffset(40);
        layoutOuterRoot(CONTAINER_WIDTH, CONTAINER_HEIGHT);
        assertEquals(40, mController.getContainerBottomMargin());
        assertEquals(CONTAINER_HEIGHT - 40, mController.getContainerHeight());

        // 7. Expand to FULL then hide: restores anchored state with latest bottomControlsOffset.
        mController.getBottomSheetForTesting().setSheetState(SheetState.FULL, /* animate= */ false);
        assertFalse(mController.isAnchoredToBottomControls());
        assertEquals(1.0f, mSheetContainer.getZ(), MathUtils.EPSILON);
        assertEquals(0, mController.getContainerBottomMargin());

        mController.hideContent(modalContent, /* animate= */ false);
        layoutOuterRoot(CONTAINER_WIDTH, CONTAINER_HEIGHT);

        assertNull(mController.getCurrentSheetContent());
        assertEquals(SheetState.HIDDEN, mController.getSheetState());
        assertTrue(mController.isAnchoredToBottomControls());
        assertEquals(0.0f, mSheetContainer.getZ(), MathUtils.EPSILON);
        assertEquals(40, mController.getContainerBottomMargin());
        assertEquals(View.GONE, mSheetContainer.getVisibility());
    }

    @Test
    public void testControllerAndCoordinator_CoversBottomControlsAndNonModalAnchoring() {
        mE2eBottomInsetDp = 24;
        mController.runSheetInitializerForTesting();
        mController.setBottomControlsOffset(56);
        layoutOuterRoot(CONTAINER_WIDTH, CONTAINER_HEIGHT);

        // 1. Unscrimmed sheet with coversBottomControls() == true covers immediately in PEEK.
        TestSheetContent coversSheet = new TestSheetContent(mActivity);
        coversSheet.mPeekHeight = 100;
        coversSheet.mCoversBottomControls = true;
        coversSheet.mHasCustomScrimLifecycle = true;

        assertTrue(mController.requestShowContent(coversSheet, /* animate= */ false));
        layoutOuterRoot(CONTAINER_WIDTH, CONTAINER_HEIGHT);

        BottomSheetCoordinator coordinator = mController.getBottomSheetForTesting();
        assertEquals(SheetState.PEEK, mController.getSheetState());
        assertFalse(mController.isSheetOpen());
        assertFalse(mController.isAnchoredToBottomControls());
        assertEquals(1.0f, mSheetContainer.getZ(), MathUtils.EPSILON);
        assertEquals(0, mController.getContainerBottomMargin());
        assertEquals(CONTAINER_HEIGHT, mController.getContainerHeight());
        assertEquals(24, mController.getEdgeToEdgeBottomInset());
        assertEquals(24, coordinator.getContentBottomPadding());
        assertEquals(
                CONTAINER_HEIGHT - 100f - 24f,
                coordinator.getView().getTranslationY(),
                MathUtils.EPSILON);

        // Expanding coversSheet to FULL keeps Z=1 and margin=0, and drops E2E bottom padding to 0.
        coordinator.setSheetState(SheetState.FULL, /* animate= */ false);
        assertFalse(mController.isAnchoredToBottomControls());
        assertEquals(1.0f, mSheetContainer.getZ(), MathUtils.EPSILON);
        assertEquals(0, mController.getContainerBottomMargin());
        assertEquals(0, coordinator.getContentBottomPadding());
        assertEquals(0f, coordinator.getView().getTranslationY(), MathUtils.EPSILON);

        mController.setBottomControlsOffset(72);
        assertEquals(0, mController.getContainerBottomMargin());
        assertEquals(1.0f, mSheetContainer.getZ(), MathUtils.EPSILON);

        // Hiding coversSheet restores anchored state and applies the updated offset (72).
        mController.hideContent(coversSheet, /* animate= */ false);
        assertTrue(mController.isAnchoredToBottomControls());
        assertEquals(0.0f, mSheetContainer.getZ(), MathUtils.EPSILON);
        assertEquals(72, mController.getContainerBottomMargin());
        assertEquals(72, ((MarginLayoutParams) mSheetContainer.getLayoutParams()).bottomMargin);

        // 2. Non-modal sheet with coversBottomControls() == false stays anchored even in HALF/FULL.
        TestSheetContent nonModalSheet = new TestSheetContent(mActivity);
        nonModalSheet.mPeekHeight = 100;
        nonModalSheet.mCoversBottomControls = false;
        nonModalSheet.mHasCustomScrimLifecycle = true;

        assertTrue(mController.requestShowContent(nonModalSheet, /* animate= */ false));
        layoutOuterRoot(CONTAINER_WIDTH, CONTAINER_HEIGHT);
        assertTrue(mController.isAnchoredToBottomControls());
        assertEquals(0.0f, mSheetContainer.getZ(), MathUtils.EPSILON);
        assertEquals(72, mController.getContainerBottomMargin());
        assertEquals(CONTAINER_HEIGHT - 72, mController.getContainerHeight());

        mController.expandSheet(/* animate= */ false);
        assertEquals(SheetState.HALF, mController.getSheetState());
        assertTrue(mController.isSheetOpen());
        assertTrue(mController.isAnchoredToBottomControls());
        assertEquals(0.0f, mSheetContainer.getZ(), MathUtils.EPSILON);
        assertEquals(72, mController.getContainerBottomMargin());
        assertEquals(0, mController.getEdgeToEdgeBottomInset());

        coordinator.setSheetState(SheetState.FULL, /* animate= */ false);
        assertEquals(SheetState.FULL, mController.getSheetState());
        assertTrue(mController.isAnchoredToBottomControls());
        assertEquals(0.0f, mSheetContainer.getZ(), MathUtils.EPSILON);
        assertEquals(72, mController.getContainerBottomMargin());
    }

    @Test
    public void testControllerAndCoordinator_QueuedSwapAndSuppressionLifecycle() {
        mController.runSheetInitializerForTesting();
        mController.setBottomControlsOffset(56);
        layoutOuterRoot(CONTAINER_WIDTH, CONTAINER_HEIGHT);

        TestSheetContent lowPriorityCoversSheet = new TestSheetContent(mActivity);
        lowPriorityCoversSheet.mPriority = ContentPriority.LOW;
        lowPriorityCoversSheet.mPeekHeight = 100;
        lowPriorityCoversSheet.mCoversBottomControls = true;
        lowPriorityCoversSheet.mHasCustomScrimLifecycle = true;

        TestSheetContent highPriorityNonModalSheet = new TestSheetContent(mActivity);
        highPriorityNonModalSheet.mPriority = ContentPriority.HIGH;
        highPriorityNonModalSheet.mPeekHeight = 100;
        highPriorityNonModalSheet.mCoversBottomControls = false;
        highPriorityNonModalSheet.mHasCustomScrimLifecycle = true;

        // 1. Show low-priority coversBottomControls sheet in PEEK -> Z=1, margin=0.
        assertTrue(mController.requestShowContent(lowPriorityCoversSheet, /* animate= */ false));
        assertEquals(lowPriorityCoversSheet, mController.getCurrentSheetContent());
        assertFalse(mController.isAnchoredToBottomControls());
        assertEquals(1.0f, mSheetContainer.getZ(), MathUtils.EPSILON);
        assertEquals(0, mController.getContainerBottomMargin());

        // 2. Preempt with high-priority non-modal sheet -> re-queues first sheet and anchors Z=0.
        assertTrue(mController.requestShowContent(highPriorityNonModalSheet, /* animate= */ false));
        mController.endAnimationsForTesting();
        assertEquals(highPriorityNonModalSheet, mController.getCurrentSheetContent());
        assertEquals(0, lowPriorityCoversSheet.mDestroyCount);
        assertTrue(mController.isAnchoredToBottomControls());
        assertEquals(0.0f, mSheetContainer.getZ(), MathUtils.EPSILON);
        assertEquals(56, mController.getContainerBottomMargin());

        // 3. Suppress and unsuppress the active sheet -> restores state without losing queue.
        mController.expandSheet(/* animate= */ false);
        assertEquals(SheetState.HALF, mController.getSheetState());
        int token = mController.suppressSheet(StateChangeReason.NONE);
        assertEquals(SheetState.HIDDEN, mController.getSheetState());

        mController.unsuppressSheet(token);
        mController.endAnimationsForTesting();
        assertEquals(highPriorityNonModalSheet, mController.getCurrentSheetContent());
        assertEquals(SheetState.HALF, mController.getSheetState());

        // 4. Hide high-priority sheet -> automatically pops and shows low-priority coversSheet.
        mController.hideContent(highPriorityNonModalSheet, /* animate= */ false);
        mController.endAnimationsForTesting();
        assertEquals(1, highPriorityNonModalSheet.mDestroyCount);
        assertEquals(lowPriorityCoversSheet, mController.getCurrentSheetContent());
        assertEquals(SheetState.PEEK, mController.getSheetState());
        assertFalse(mController.isAnchoredToBottomControls());
        assertEquals(1.0f, mSheetContainer.getZ(), MathUtils.EPSILON);
        assertEquals(0, mController.getContainerBottomMargin());
    }

    @Test
    public void testLargeFormFactor_MultiHopLayoutModeAndMarginTransitions() {
        BottomSheetCoordinator sheet =
                buildCoordinator(
                        /* isLargeFormFactor= */ true,
                        /* alwaysFullWidth= */ false,
                        CONTAINER_WIDTH,
                        CONTAINER_HEIGHT);

        int desktopMargin =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_desktop_bottom_margin);
        int lffWidth =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_large_form_factor_width);
        int lffEdgeGap =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_large_form_factor_edge_gap);
        int narrowThreshold =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_narrow_width_threshold);
        int narrowWidth =
                mActivity.getResources().getDimensionPixelSize(R.dimen.bottom_sheet_narrow_width);

        List<Integer> marginNotifications = new ArrayList<>();
        sheet.addObserver(
                new BottomSheetObserver() {
                    @Override
                    public void onContainerBottomMarginChanged(int bottomMargin) {
                        marginNotifications.add(bottomMargin);
                    }
                });

        sheet.setBottomMargin(40);
        assertEquals(40, sheet.getContainerBottomMargin());
        assertEquals(List.of(40), marginNotifications);

        TestSheetContent popupContent = new TestSheetContent(mActivity);
        popupContent.mSupportsLargeFormFactor = true;
        popupContent.mHasCustomScrimLifecycle = true;

        TestSheetContent fallbackContent = new TestSheetContent(mActivity);
        fallbackContent.mSupportsLargeFormFactor = false;
        fallbackContent.mHasCustomScrimLifecycle = true;

        // Hop 1: null -> DESKTOP_POPUP.
        sheet.showContent(popupContent);
        sheet.setSheetState(SheetState.FULL, /* animate= */ false);

        assertEquals(SheetLayoutMode.DESKTOP_POPUP, sheet.getView().getSheetLayoutMode());
        assertTrue(sheet.isLargeFormFactorUiEnabled());
        assertEquals(40 + desktopMargin, sheet.getContainerBottomMargin());
        assertEquals(
                40 + desktopMargin,
                ((MarginLayoutParams) mSheetContainer.getLayoutParams()).bottomMargin);
        assertEquals(List.of(40, 40 + desktopMargin), marginNotifications);
        assertEquals(
                View.VISIBLE,
                sheet.getView().findViewById(R.id.bottom_sheet_close_button).getVisibility());
        assertFalse(mSheetContainer.getClipChildren());
        assertFalse(mSheetContainer.getClipToPadding());
        assertFalse(mOuterRoot.getClipChildren());
        assertFalse(mOuterRoot.getClipToPadding());
        assertEquals(CONTAINER_HEIGHT - desktopMargin, sheet.getMaxSheetHeight());
        assertEquals(lffWidth, sheet.getMaxSheetWidth());
        assertFalse(sheet.isFullWidth());
        assertEquals(
                (CONTAINER_WIDTH - lffWidth) / 2f,
                sheet.getView().getTranslationX(),
                MathUtils.EPSILON);

        // Verify RTL horizontal centering and narrow-window 2 * edgeGap width clamping in POPUP.
        LocalizationUtils.setRtlForTesting(true);
        layoutOuterRoot(1200, CONTAINER_HEIGHT);
        assertEquals(-(1200 - lffWidth) / 2f, sheet.getView().getTranslationX(), MathUtils.EPSILON);

        LocalizationUtils.setRtlForTesting(false);
        layoutOuterRoot(400, CONTAINER_HEIGHT);
        assertEquals(400 - 2 * lffEdgeGap, sheet.getMaxSheetWidth());
        assertEquals(lffEdgeGap, sheet.getView().getTranslationX(), MathUtils.EPSILON);
        layoutOuterRoot(CONTAINER_WIDTH, CONTAINER_HEIGHT);

        // Hop 2: DESKTOP_POPUP -> DESKTOP_FALLBACK (preserves caller margin 40, removes 24dp).
        sheet.showContent(fallbackContent);
        sheet.setSheetState(SheetState.FULL, /* animate= */ false);
        layoutOuterRoot(CONTAINER_WIDTH, CONTAINER_HEIGHT);

        assertEquals(SheetLayoutMode.DESKTOP_FALLBACK, sheet.getView().getSheetLayoutMode());
        assertFalse(sheet.isLargeFormFactorUiEnabled());
        assertEquals(40, sheet.getContainerBottomMargin());
        assertEquals(40, ((MarginLayoutParams) mSheetContainer.getLayoutParams()).bottomMargin);
        assertEquals(List.of(40, 40 + desktopMargin, 40), marginNotifications);
        assertEquals(
                View.GONE,
                sheet.getView().findViewById(R.id.bottom_sheet_close_button).getVisibility());
        assertFalse(mSheetContainer.getClipChildren());
        assertTrue(mSheetContainer.getClipToPadding());
        assertTrue(mOuterRoot.getClipChildren());
        assertTrue(mOuterRoot.getClipToPadding());
        assertEquals(CONTAINER_HEIGHT - 40, sheet.getMaxSheetHeight());
        assertTrue(CONTAINER_WIDTH > narrowThreshold);
        assertEquals(narrowWidth, sheet.getMaxSheetWidth());

        // Hop 3: Update caller margin in DESKTOP_FALLBACK (64), then switch back to DESKTOP_POPUP.
        sheet.setBottomMargin(64);
        assertEquals(64, sheet.getContainerBottomMargin());
        assertEquals(List.of(40, 40 + desktopMargin, 40, 64), marginNotifications);

        sheet.showContent(popupContent);
        assertEquals(SheetLayoutMode.DESKTOP_POPUP, sheet.getView().getSheetLayoutMode());
        assertEquals(64 + desktopMargin, sheet.getContainerBottomMargin());
        assertEquals(
                List.of(40, 40 + desktopMargin, 40, 64, 64 + desktopMargin), marginNotifications);

        // Re-setting identical caller margin (64) does not fire a duplicate observer notification.
        sheet.setBottomMargin(64);
        assertEquals(5, marginNotifications.size());

        // Hop 4: DESKTOP_POPUP -> STANDARD via showContent(null).
        sheet.showContent(null);
        assertEquals(SheetLayoutMode.STANDARD, sheet.getView().getSheetLayoutMode());
        assertFalse(sheet.isLargeFormFactorUiEnabled());
        assertEquals(64, sheet.getContainerBottomMargin());
        assertEquals(
                List.of(40, 40 + desktopMargin, 40, 64, 64 + desktopMargin, 64),
                marginNotifications);
        assertEquals(
                View.GONE,
                sheet.getView().findViewById(R.id.bottom_sheet_close_button).getVisibility());
        assertTrue(mSheetContainer.getClipToPadding());
        assertTrue(mOuterRoot.getClipChildren());
    }

    @Test
    public void testWrapContent_WidthReflowMidAnimationAndE2eClamping() {
        mE2eBottomInsetDp = 24;
        BottomSheetCoordinator sheet =
                buildCoordinator(
                        /* isLargeFormFactor= */ false,
                        /* alwaysFullWidth= */ false,
                        /* width= */ 500,
                        /* height= */ 1000);

        int narrowWidth =
                mActivity.getResources().getDimensionPixelSize(R.dimen.bottom_sheet_narrow_width);
        ReflowContentView reflowView =
                new ReflowContentView(mActivity, /* narrowWidthCutoff= */ 500);
        TestSheetContent wrapContent = new TestSheetContent(mActivity);
        wrapContent.setContentView(reflowView);
        wrapContent.mFullRatio = HeightMode.WRAP_CONTENT;
        wrapContent.mHalfRatio = HeightMode.DISABLED;
        wrapContent.mPeekHeight = 80;
        wrapContent.mShowHandlebar = true;

        // 1. Open in PEEK at width=500, mutate content height while in PEEK, then expand to FULL.
        sheet.showContent(wrapContent);
        sheet.setSheetState(SheetState.PEEK, /* animate= */ false);
        assertEquals(SheetState.PEEK, sheet.getSheetState());

        reflowView.updateExtraHeightAndRelayout(
                40, /* layoutWidth= */ 500, /* layoutHeight= */ 360);
        // Layout change in PEEK invalidates cached desired height without leaving PEEK.
        assertEquals(SheetState.PEEK, sheet.getSheetState());

        sheet.setSheetState(SheetState.FULL, /* animate= */ false);
        int handlebarHeight = sheet.getView().getHandlebarForTesting().getMeasuredHeight();
        assertEquals(500, reflowView.mLastMeasuredWidth);
        assertEquals(1000 - handlebarHeight - 24, reflowView.mLastMaxHeightSpec);
        assertEquals(
                360f + handlebarHeight,
                sheet.getSheetHeightForState(SheetState.FULL),
                MathUtils.EPSILON);
        assertEquals(360f + handlebarHeight, sheet.getCurrentOffsetPx(), MathUtils.EPSILON);
        assertEquals(
                1000f - (360f + handlebarHeight) - 24f,
                sheet.getView().getTranslationY(),
                MathUtils.EPSILON);

        // 2. Widen container from 500 to 1080 at constant height 1000 -> reflows to narrowWidth.
        layoutOuterRoot(CONTAINER_WIDTH, 1000);
        assertEquals(narrowWidth, sheet.getMaxSheetWidth());
        assertEquals(narrowWidth, reflowView.mLastMeasuredWidth);
        // At narrowWidth (> 500), baseHeight is 200 + extraHeight 40 = 240.
        assertEquals(
                240f + handlebarHeight,
                sheet.getSheetHeightForState(SheetState.FULL),
                MathUtils.EPSILON);

        // 3. Mid-animation content layout change restarts settle animation to updated height.
        sheet.setSheetState(SheetState.PEEK, /* animate= */ false);
        assertEquals(80f + handlebarHeight, sheet.getCurrentOffsetPx(), MathUtils.EPSILON);

        sheet.setSheetState(SheetState.FULL, /* animate= */ true);
        assertEquals(SheetState.SCROLLING, sheet.getSheetState());
        assertEquals(SheetState.FULL, sheet.getTargetSheetState());

        reflowView.updateExtraHeightAndRelayout(
                110, /* layoutWidth= */ narrowWidth, /* layoutHeight= */ 310);
        assertEquals(SheetState.SCROLLING, sheet.getSheetState());
        assertEquals(SheetState.FULL, sheet.getTargetSheetState());

        sheet.endAnimationsForTesting();
        assertEquals(SheetState.FULL, sheet.getSheetState());
        assertEquals(310f + handlebarHeight, sheet.getCurrentOffsetPx(), MathUtils.EPSILON);

        // 4. Content exceeding viewport height is clamped to maxSheetHeight - bottomPadding.
        reflowView.updateExtraHeightAndRelayout(
                2000, /* layoutWidth= */ narrowWidth, /* layoutHeight= */ 2200);
        sheet.endAnimationsForTesting();
        assertEquals(1000f - 24f, sheet.getSheetHeightForState(SheetState.FULL), MathUtils.EPSILON);
        assertEquals(1000f - 24f, sheet.getCurrentOffsetPx(), MathUtils.EPSILON);
        assertEquals(0f, sheet.getView().getTranslationY(), MathUtils.EPSILON);
    }

    @Test
    public void testWrapContent_DetachedContentViewLayoutDoesNotInvalidateCurrentSheet() {
        BottomSheetCoordinator sheet =
                buildCoordinator(
                        /* isLargeFormFactor= */ false,
                        /* alwaysFullWidth= */ false,
                        /* width= */ 500,
                        /* height= */ 1000);

        ReflowContentView viewA = new ReflowContentView(mActivity, /* narrowWidthCutoff= */ 500);
        viewA.setNarrowBaseHeight(220);
        TestSheetContent contentA = new TestSheetContent(mActivity);
        contentA.setContentView(viewA);
        contentA.mFullRatio = HeightMode.WRAP_CONTENT;
        contentA.mHalfRatio = HeightMode.DISABLED;

        ReflowContentView viewB = new ReflowContentView(mActivity, /* narrowWidthCutoff= */ 500);
        viewB.setNarrowBaseHeight(340);
        TestSheetContent contentB = new TestSheetContent(mActivity);
        contentB.setContentView(viewB);
        contentB.mFullRatio = HeightMode.WRAP_CONTENT;
        contentB.mHalfRatio = HeightMode.DISABLED;

        sheet.showContent(contentA);
        sheet.setSheetState(SheetState.FULL, /* animate= */ false);
        assertEquals(220f, sheet.getCurrentOffsetPx(), MathUtils.EPSILON);

        sheet.showContent(contentB);
        sheet.setSheetState(SheetState.FULL, /* animate= */ false);
        assertEquals(340f, sheet.getCurrentOffsetPx(), MathUtils.EPSILON);
        int measuresBeforeDetachedLayout = viewB.mMeasureCount;

        // Laying out detached viewA must not trigger onLayoutChange or re-measure viewB.
        viewA.setNarrowBaseHeight(600);
        viewA.layout(0, 0, 500, 600);
        assertEquals(340f, sheet.getCurrentOffsetPx(), MathUtils.EPSILON);
        assertEquals(measuresBeforeDetachedLayout, viewB.mMeasureCount);
    }

    @Test
    public void testCoversBottomControls_AnimatedCollapseAndHideE2eInsetTransitions() {
        mE2eBottomInsetDp = 24;
        BottomSheetCoordinator sheet =
                buildCoordinator(
                        /* isLargeFormFactor= */ false,
                        /* alwaysFullWidth= */ false,
                        CONTAINER_WIDTH,
                        /* height= */ 1000);

        WindowInsetsCompat insets =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.statusBars(), Insets.of(0, 30, 0, 0))
                        .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.of(0, 0, 0, 48))
                        .build();
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(insets);

        TestSheetContent coversContent = new TestSheetContent(mActivity);
        coversContent.mCoversBottomControls = true;
        coversContent.mPeekHeight = 90;
        coversContent.mHalfRatio = 0.5f;
        coversContent.mFullRatio = MAX_HEIGHT_RATIO;

        TouchRestrictingFrameLayout contentContainer =
                sheet.getView().findViewById(R.id.bottom_sheet_content);

        // 1. Settled in FULL: effective E2E bottom inset is 0; status bar (30) is preserved while
        // navigation bar (48) is excluded from visible viewport bottom.
        sheet.showContent(coversContent);
        sheet.setSheetState(SheetState.FULL, /* animate= */ false);

        assertEquals(SheetState.FULL, sheet.getSheetState());
        assertEquals(SheetState.NONE, sheet.getTargetSheetState());
        assertEquals(0, sheet.getContentBottomPadding());
        assertEquals(0, contentContainer.getPaddingBottom());
        assertEquals(1000f, sheet.getCurrentOffsetPx(), MathUtils.EPSILON);
        assertEquals(0f, sheet.getView().getTranslationY(), MathUtils.EPSILON);
        assertEquals(30, sheet.getVisibleViewportRectForTesting().top);
        int expectedViewportBottom =
                1000
                        - insets.getInsets(
                                        WindowInsetsCompat.Type.ime()
                                                | WindowInsetsCompat.Type.statusBars())
                                .bottom;
        assertEquals(expectedViewportBottom, sheet.getVisibleViewportRectForTesting().bottom);

        // 2. Start animated collapse FULL -> PEEK (state=SCROLLING, target=PEEK): effective E2E
        // bottom inset (24) activates immediately during the collapse animation.
        sheet.setSheetState(SheetState.PEEK, /* animate= */ true);
        assertEquals(SheetState.SCROLLING, sheet.getSheetState());
        assertEquals(SheetState.PEEK, sheet.getTargetSheetState());

        sheet.setSheetOffsetFromBottom(500f, StateChangeReason.NONE);
        assertEquals(24, sheet.getContentBottomPadding());
        assertEquals(24, contentContainer.getPaddingBottom());
        assertEquals(1000f - 500f - 24f, sheet.getView().getTranslationY(), MathUtils.EPSILON);

        // 3. Settle in PEEK (state=PEEK, target=NONE): effective E2E bottom inset remains 24.
        sheet.endAnimationsForTesting();
        assertEquals(SheetState.PEEK, sheet.getSheetState());
        assertEquals(SheetState.NONE, sheet.getTargetSheetState());
        assertEquals(24, sheet.getContentBottomPadding());
        assertEquals(24, contentContainer.getPaddingBottom());
        assertEquals(90f, sheet.getCurrentOffsetPx(), MathUtils.EPSILON);
        assertEquals(1000f - 90f - 24f, sheet.getView().getTranslationY(), MathUtils.EPSILON);

        // 4. Start animated hide PEEK -> HIDDEN (state=SCROLLING, target=HIDDEN): effective E2E
        // bottom inset drops back to 0 during the hide animation.
        sheet.setSheetState(SheetState.HIDDEN, /* animate= */ true);
        assertEquals(SheetState.SCROLLING, sheet.getSheetState());
        assertEquals(SheetState.HIDDEN, sheet.getTargetSheetState());

        sheet.setSheetOffsetFromBottom(45f, StateChangeReason.NONE);
        assertEquals(0, sheet.getContentBottomPadding());
        assertEquals(0, contentContainer.getPaddingBottom());
        assertEquals(1000f - 45f, sheet.getView().getTranslationY(), MathUtils.EPSILON);

        sheet.endAnimationsForTesting();
        assertEquals(SheetState.HIDDEN, sheet.getSheetState());
        assertEquals(SheetState.NONE, sheet.getTargetSheetState());
        assertEquals(0f, sheet.getCurrentOffsetPx(), MathUtils.EPSILON);
        assertEquals(1000f, sheet.getView().getTranslationY(), MathUtils.EPSILON);

        // 5. Non-covering content includes navigationBars (48) in visibleViewportRect.bottom.
        TestSheetContent nonCoveringContent = new TestSheetContent(mActivity);
        nonCoveringContent.mCoversBottomControls = false;
        sheet.showContent(nonCoveringContent);
        assertEquals(30, sheet.getVisibleViewportRectForTesting().top);
        assertEquals(1000 - 48, sheet.getVisibleViewportRectForTesting().bottom);
    }

    @Test
    public void testAlwaysFullWidth_AndBrowserControlsHiddenRatio() {
        BottomSheetCoordinator sheet =
                buildCoordinator(
                        /* isLargeFormFactor= */ false,
                        /* alwaysFullWidth= */ true,
                        CONTAINER_WIDTH,
                        CONTAINER_HEIGHT);

        TestSheetContent hideOnScrollContent = new TestSheetContent(mActivity);
        hideOnScrollContent.mHideOnScroll = true;
        hideOnScrollContent.mPeekHeight = 100;
        hideOnScrollContent.mHalfRatio = 0.5f;
        hideOnScrollContent.mFullRatio = MAX_HEIGHT_RATIO;

        sheet.showContent(hideOnScrollContent);
        sheet.setSheetState(SheetState.PEEK, /* animate= */ false);

        // 1. alwaysFullWidth forces full container width even when width > narrowWidthThreshold.
        assertEquals(CONTAINER_WIDTH, sheet.getMaxSheetWidth());
        assertTrue(sheet.isFullWidth());
        assertEquals(0f, sheet.getView().getTranslationX(), MathUtils.EPSILON);

        // 2. Hiding browser controls by 50% in PEEK pushes the sheet down by 50% of peek height.
        sheet.setBrowserControlsHiddenRatio(0.5f);
        assertEquals(
                CONTAINER_HEIGHT - (100f - 50f),
                sheet.getView().getTranslationY(),
                MathUtils.EPSILON);

        // 3. During opening animation (SCROLLING -> PEEK) with alwaysFullWidth and hiddenRatio=1.0,
        // getOffsetFromBrowserControls() uses targetState (PEEK) so sheet stays offscreen.
        sheet.setSheetState(SheetState.HIDDEN, /* animate= */ false);
        sheet.setBrowserControlsHiddenRatio(1.0f);
        sheet.setSheetState(SheetState.PEEK, /* animate= */ true);
        assertEquals(SheetState.SCROLLING, sheet.getSheetState());
        assertEquals(SheetState.PEEK, sheet.getTargetSheetState());

        sheet.setSheetOffsetFromBottom(60f, StateChangeReason.NONE);
        assertEquals(
                CONTAINER_HEIGHT - (60f - 100f),
                sheet.getView().getTranslationY(),
                MathUtils.EPSILON);

        sheet.endAnimationsForTesting();
        sheet.setBrowserControlsHiddenRatio(0f);
        assertEquals(SheetState.PEEK, sheet.getSheetState());
        assertEquals(CONTAINER_HEIGHT - 100f, sheet.getView().getTranslationY(), MathUtils.EPSILON);
    }

    @Test
    public void testResizeContent_DynamicSplitScreenTransitionAndSwipeTargetState() {
        int minHalfFullDistance =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bottom_sheet_min_full_half_distance);
        int tallHeight = minHalfFullDistance * 4;
        int shortHeight = minHalfFullDistance;

        BottomSheetCoordinator sheet =
                buildCoordinator(
                        /* isLargeFormFactor= */ false,
                        /* alwaysFullWidth= */ false,
                        CONTAINER_WIDTH,
                        tallHeight);

        TestSheetContent resizeContent = new TestSheetContent(mActivity);
        resizeContent.mFullRatio = HeightMode.RESIZE_CONTENT;
        resizeContent.mMaxResizeContentHeightRatio = 0.8f;
        resizeContent.mHalfRatio = 0.5f;
        resizeContent.mPeekHeight = 60;
        resizeContent.mSwipeToDismissEnabled = false;

        TouchRestrictingFrameLayout contentContainer =
                sheet.getView().findViewById(R.id.bottom_sheet_content);

        // 1. Tall container: isSmallScreen() is false, HALF is enabled, RESIZE_CONTENT is active.
        sheet.showContent(resizeContent);
        sheet.setSheetState(SheetState.HALF, /* animate= */ false);

        assertFalse(sheet.isSmallScreen());
        assertEquals(SheetState.HALF, sheet.getSheetState());
        assertEquals(0.5f, sheet.getHalfRatio(), MathUtils.EPSILON);
        assertEquals(0.8f, sheet.getFullRatio(), MathUtils.EPSILON);
        assertEquals((int) (0.5f * tallHeight), contentContainer.getLayoutParams().height);

        // Dragging between HALF (0.5) and FULL (0.8) dynamically resizes the content container.
        float midOffset = 0.65f * tallHeight;
        sheet.setSheetOffset(midOffset, /* shouldAnimate= */ false);
        assertEquals(SheetState.SCROLLING, sheet.getSheetState());
        assertEquals((int) midOffset, contentContainer.getLayoutParams().height);

        // Releasing near HALF (0.55 * tallHeight) with zero velocity settles back to HALF.
        float nearHalfOffset = 0.55f * tallHeight;
        sheet.setSheetOffset(nearHalfOffset, /* shouldAnimate= */ false);
        sheet.setSheetOffset(nearHalfOffset, /* shouldAnimate= */ true);
        assertEquals(SheetState.HALF, sheet.getTargetSheetState());
        sheet.endAnimationsForTesting();
        assertEquals(SheetState.HALF, sheet.getSheetState());

        // 2. Shrink container to short split-screen height: isSmallScreen() becomes true, HALF
        // promotes to FULL, RESIZE_CONTENT deactivates (fullRatio=1.0, height=MATCH_PARENT), and
        // swipe release at 0.55 * shortHeight targets FULL instead of HALF.
        layoutOuterRoot(CONTAINER_WIDTH, shortHeight);

        assertTrue(sheet.isSmallScreen());
        assertEquals(SheetState.FULL, sheet.getSheetState());
        assertEquals(0f, sheet.getHalfRatio(), MathUtils.EPSILON);
        assertEquals(MAX_HEIGHT_RATIO, sheet.getFullRatio(), MathUtils.EPSILON);
        assertEquals(
                ViewGroup.LayoutParams.MATCH_PARENT, contentContainer.getLayoutParams().height);

        float shortMidOffset = 0.55f * shortHeight;
        sheet.setSheetOffset(shortMidOffset, /* shouldAnimate= */ false);
        sheet.setSheetOffset(shortMidOffset, /* shouldAnimate= */ true);
        assertEquals(SheetState.FULL, sheet.getTargetSheetState());
        sheet.endAnimationsForTesting();
        assertEquals(SheetState.FULL, sheet.getSheetState());
    }

    private void layoutOuterRoot(int width, int height) {
        mActivity.getWindow().getDecorView().layout(0, 0, width, height);
        ViewUtils.requestLayout(mSheetContainer, "BottomSheetLayoutTest");
        mOuterRoot.measure(
                MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));
        mOuterRoot.layout(0, 0, width, height);
    }

    private BottomSheetCoordinator buildCoordinator(
            boolean isLargeFormFactor, boolean alwaysFullWidth, int width, int height) {
        int layoutId = isLargeFormFactor ? R.layout.bottom_sheet_desktop : R.layout.bottom_sheet;
        BottomSheetView view =
                (BottomSheetView)
                        LayoutInflater.from(mActivity).inflate(layoutId, /* root= */ null);
        mSheetContainer.removeAllViews();
        MarginLayoutParams params = (MarginLayoutParams) mSheetContainer.getLayoutParams();
        params.topMargin = 0;
        params.bottomMargin = 0;
        mSheetContainer.setLayoutParams(params);
        mSheetContainer.addView(view);
        layoutOuterRoot(width, height);

        BottomSheetCoordinator sheet = new BottomSheetCoordinator(view);
        sheet.init(
                mActivity.getWindow(),
                mKeyboardDelegate,
                alwaysFullWidth,
                () -> mE2eBottomInsetDp,
                /* appHeaderHeight= */ 0,
                /* bottomMargin= */ 0,
                mInsetObserver,
                isLargeFormFactor);
        mCoordinatorsToDestroy.add(sheet);
        return sheet;
    }

    /** Configurable {@link BottomSheetContent} backed by a real {@link View}. */
    private static class TestSheetContent implements BottomSheetContent {
        private View mContentView;
        int mPriority = ContentPriority.HIGH;
        int mPeekHeight = HeightMode.DISABLED;
        float mHalfRatio = 0.5f;
        float mFullRatio = MAX_HEIGHT_RATIO;
        float mMaxResizeContentHeightRatio = MAX_HEIGHT_RATIO;
        boolean mCoversBottomControls;
        boolean mHasCustomScrimLifecycle;
        boolean mSupportsLargeFormFactor = true;
        boolean mSwipeToDismissEnabled = true;
        boolean mShowHandlebar;
        boolean mHideOnScroll;
        int mDestroyCount;
        final SettableNonNullObservableSupplier<Boolean> mBackPressStateChangedSupplier =
                ObservableSuppliers.createNonNull(false);

        TestSheetContent(Activity activity) {
            mContentView = new FrameLayout(activity);
        }

        void setContentView(View contentView) {
            mContentView = contentView;
        }

        @Override
        public View getContentView() {
            return mContentView;
        }

        @Override
        public @Nullable View getToolbarView() {
            return null;
        }

        @Override
        public int getVerticalScrollOffset() {
            return 0;
        }

        @Override
        public void destroy() {
            mDestroyCount++;
        }

        @Override
        public BottomSheetType getSheetType() {
            return new BottomSheetType.Builder()
                    .setModal(!mHasCustomScrimLifecycle)
                    .setUserInitiated(mPriority == ContentPriority.HIGH)
                    .setSuppressible(mPriority == ContentPriority.LOW)
                    .build();
        }

        @Override
        public int getPriority() {
            return mPriority;
        }

        @Override
        public boolean swipeToDismissEnabled() {
            return mSwipeToDismissEnabled;
        }

        @Override
        public int getPeekHeight() {
            return mPeekHeight;
        }

        @Override
        public float getHalfHeightRatio() {
            return mHalfRatio;
        }

        @Override
        public float getFullHeightRatio() {
            return mFullRatio;
        }

        @Override
        public float getMaxResizeContentHeightRatio() {
            return mMaxResizeContentHeightRatio;
        }

        @Override
        public boolean coversBottomControls() {
            return mCoversBottomControls;
        }

        @Override
        public boolean hasCustomScrimLifecycle() {
            return mHasCustomScrimLifecycle;
        }

        @Override
        public boolean supportsLargeFormFactor() {
            return mSupportsLargeFormFactor;
        }

        @Override
        public boolean showHandlebar() {
            return mShowHandlebar;
        }

        @Override
        public boolean hideOnScroll() {
            return mHideOnScroll;
        }

        @Override
        public NonNullObservableSupplier<Boolean> getBackPressStateChangedSupplier() {
            return mBackPressStateChangedSupplier;
        }

        @Override
        public int getSheetHalfHeightAccessibilityStringId() {
            return android.R.string.ok;
        }

        @Override
        public int getSheetFullHeightAccessibilityStringId() {
            return android.R.string.ok;
        }

        @Override
        public int getSheetClosedAccessibilityStringId() {
            return android.R.string.copy;
        }

        @Override
        public int getSheetHiddenAccessibilityStringId() {
            return android.R.string.cancel;
        }
    }

    /**
     * Width-responsive content view that simulates multi-line text reflow when measured at narrower
     * widths vs wider widths.
     */
    private static class ReflowContentView extends View {
        private final int mNarrowWidthCutoff;
        int mNarrowBaseHeight = 320;
        private static final int WIDE_BASE_HEIGHT = 200;
        int mExtraHeight;
        int mLastMeasuredWidth;
        int mLastMaxHeightSpec;
        int mMeasureCount;

        ReflowContentView(Activity activity, int narrowWidthCutoff) {
            super(activity);
            mNarrowWidthCutoff = narrowWidthCutoff;
        }

        void updateExtraHeightAndRelayout(int extraHeight, int layoutWidth, int layoutHeight) {
            mExtraHeight = extraHeight;
            ViewUtils.requestLayout(this, "BottomSheetLayoutTest");
            layout(0, 0, layoutWidth, layoutHeight);
            ViewUtils.requestLayout(this, "BottomSheetLayoutTest");
        }

        void setNarrowBaseHeight(int narrowBaseHeight) {
            mNarrowBaseHeight = narrowBaseHeight;
            ViewUtils.requestLayout(this, "BottomSheetLayoutTest");
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            mMeasureCount++;
            int width = MeasureSpec.getSize(widthMeasureSpec);
            mLastMeasuredWidth = width;
            mLastMaxHeightSpec = MeasureSpec.getSize(heightMeasureSpec);
            int baseHeight = width <= mNarrowWidthCutoff ? mNarrowBaseHeight : WIDE_BASE_HEIGHT;
            int desiredHeight = baseHeight + mExtraHeight;
            setMeasuredDimension(width, View.resolveSize(desiredHeight, heightMeasureSpec));
        }
    }
}
