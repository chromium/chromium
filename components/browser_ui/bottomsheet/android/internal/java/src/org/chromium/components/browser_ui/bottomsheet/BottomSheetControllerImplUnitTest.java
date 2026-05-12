// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Robolectric.buildActivity;

import android.app.Activity;
import android.view.ViewGroup;
import android.view.Window;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.insets.InsetObserver;

import java.util.function.Supplier;

/** Unit tests for {@link BottomSheetControllerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BottomSheetControllerImplUnitTest {
    private static final int APP_HEADER_HEIGHT = 42;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ScrimManager mScrimManager;
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private ViewGroup mRoot;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private AppHeaderState mAppHeaderState;
    @Mock private BottomSheet mBottomSheet;
    @Mock private BottomSheetContent mSheetContent;
    @Mock private InsetObserver mInsetObserver;
    @Captor ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;

    private BottomSheetControllerImpl mController;
    private final OneshotSupplierImpl<ScrimManager> mScrimManagerSupplier =
            new OneshotSupplierImpl<>();
    private Window mWindow;
    private final OneshotSupplierImpl<ViewGroup> mRootSupplier = new OneshotSupplierImpl<>();
    private final SettableMonotonicObservableSupplier<Integer> mEdgeToEdgeBottomInsetSupplier =
            ObservableSuppliers.createMonotonic();

    @Before
    public void setUp() {
        Activity activity = buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mWindow = activity.getWindow();
        when(mRoot.getContext()).thenReturn(activity);
        when(mRoot.findViewById(R.id.bottom_sheet)).thenReturn(mBottomSheet);
        mScrimManagerSupplier.set(mScrimManager);
        mRootSupplier.set(mRoot);
        mController =
                new BottomSheetControllerImpl(
                        mScrimManagerSupplier,
                        mWindow,
                        mKeyboardVisibilityDelegate,
                        mRootSupplier,
                        false,
                        mEdgeToEdgeBottomInsetSupplier,
                        mDesktopWindowStateManager,
                        mInsetObserver);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderStateChange_SheetNotInitialized() {
        // Trigger app header state change before first sheet is triggered.
        when(mAppHeaderState.getAppHeaderHeight()).thenReturn(APP_HEADER_HEIGHT);
        mController.onAppHeaderStateChanged(mAppHeaderState);

        // Simulate sheet initialization, this should kick off
        // BottomSheet#onAppHeaderHeightChange().
        mController.runSheetInitializerForTesting();
        verify(mBottomSheet)
                .init(
                        mWindow,
                        mKeyboardVisibilityDelegate,
                        false,
                        mEdgeToEdgeBottomInsetSupplier,
                        APP_HEADER_HEIGHT,
                        0,
                        mInsetObserver);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderStateChange_SheetInitialized() {
        // Simulate sheet initialization.
        mController.runSheetInitializerForTesting();

        // Trigger app header state change after sheet is initialized.
        when(mAppHeaderState.getAppHeaderHeight()).thenReturn(APP_HEADER_HEIGHT);
        mController.onAppHeaderStateChanged(mAppHeaderState);
        verify(mBottomSheet).onAppHeaderHeightChanged(APP_HEADER_HEIGHT);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderStateChange_NoHeaderHeightChange() {
        // Simulate sheet initialization.
        mController.runSheetInitializerForTesting();

        // Trigger multiple app header state changes with no height change, after sheet is
        // initialized.
        when(mAppHeaderState.getAppHeaderHeight()).thenReturn(APP_HEADER_HEIGHT);
        when(mAppHeaderState.getUnoccludedRectWidth()).thenReturn(150);
        mController.onAppHeaderStateChanged(mAppHeaderState);
        var newAppHeaderState = mock(AppHeaderState.class);
        when(mAppHeaderState.getAppHeaderHeight()).thenReturn(APP_HEADER_HEIGHT);
        when(mAppHeaderState.getUnoccludedRectWidth()).thenReturn(100);
        mController.onAppHeaderStateChanged(newAppHeaderState);
        verify(mBottomSheet, times(1)).onAppHeaderHeightChanged(APP_HEADER_HEIGHT);
    }

    @Test
    public void testScrimZOrdering() {
        mController.runSheetInitializerForTesting();
        doReturn(true).when(mBottomSheet).isSheetOpen();

        mController.scrimVisibilityChanged(true);
        verify(mRoot).setZ(1.0f);

        mController.scrimVisibilityChanged(false);
        verify(mRoot).setZ(0.0f);
    }

    @Test
    public void testBottomControlsOffset() {
        mController.runSheetInitializerForTesting();
        doReturn(true).when(mBottomSheet).isSheetOpen();
        mController.setBottomControlsOffset(100);

        verify(mBottomSheet).setBottomMargin(100);

        mController.scrimVisibilityChanged(true);
        verify(mBottomSheet).setBottomMargin(0);
    }

    @Test
    public void testScrimStartsVisible() {
        doReturn(true).when(mScrimManager).isShowingScrim();
        mController.runSheetInitializerForTesting();
        verify(mBottomSheet).addObserver(mBottomSheetObserverCaptor.capture());

        doReturn(true).when(mBottomSheet).isSheetOpen();
        mBottomSheetObserverCaptor.getValue().onSheetOpened(StateChangeReason.NONE);
        verify(mRoot).setZ(1.0f);
    }

    @Test
    public void testHasBottomInset() {
        assertFalse(mController.hasBottomInset());

        mEdgeToEdgeBottomInsetSupplier.set(0);
        assertFalse(mController.hasBottomInset());

        mEdgeToEdgeBottomInsetSupplier.set(100);
        assertTrue(mController.hasBottomInset());
    }

    @Test
    public void testGetMaxOffset() {
        assertEquals(0, mController.getMaxOffset());

        mController.runSheetInitializerForTesting();
        doReturn(123.0f).when(mBottomSheet).getMaxOffsetPx();

        assertEquals(123, mController.getMaxOffset());
    }

    @Test
    public void testRequestShowContent_FailsIfRootViewIsNull() {
        // Create a new supplier that returns null to simulate a destroyed activity.
        Supplier<ViewGroup> nullRootSupplier = () -> null;
        BottomSheetControllerImpl controllerWithNullRoot =
                new BottomSheetControllerImpl(
                        mScrimManagerSupplier,
                        mWindow,
                        mKeyboardVisibilityDelegate,
                        nullRootSupplier,
                        false,
                        mEdgeToEdgeBottomInsetSupplier,
                        mDesktopWindowStateManager,
                        mInsetObserver);

        // Requesting to show content should fail gracefully instead of crashing.
        boolean result =
                controllerWithNullRoot.requestShowContent(mSheetContent, /* animate= */ true);

        // Verify that the request was blocked.
        assertFalse("requestShowContent should return false when the root view is null.", result);
    }

    @Test
    public void testRequestShowContent_currentLow_newHigh_canBeSuppressed_returnsFalse() {
        mController.runSheetInitializerForTesting();

        BottomSheetContent newContent = mock(BottomSheetContent.class);
        when(newContent.getPriority()).thenReturn(BottomSheetContent.ContentPriority.HIGH);
        BottomSheetContent currentContent = mock(BottomSheetContent.class);
        when(currentContent.getPriority()).thenReturn(BottomSheetContent.ContentPriority.LOW);
        when(currentContent.canBeSuppressed(newContent)).thenReturn(false);
        when(currentContent.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());

        when(mBottomSheet.getCurrentSheetContent()).thenReturn(currentContent);
        when(mBottomSheet.isSheetOpen()).thenReturn(true);

        boolean result = mController.requestShowContent(newContent, /* animate= */ true);

        assertFalse("Request should return false as content cannot be suppressed", result);
        verify(mBottomSheet, times(0)).setSheetState(SheetState.HIDDEN, true);
    }

    @Test
    public void testRequestShowContent_currentLow_newHigh_canBeSuppressed_returnsTrue() {
        mController.runSheetInitializerForTesting();

        BottomSheetContent newContent = mock(BottomSheetContent.class);
        when(newContent.getPriority()).thenReturn(BottomSheetContent.ContentPriority.HIGH);
        BottomSheetContent currentContent = mock(BottomSheetContent.class);
        when(currentContent.getPriority()).thenReturn(BottomSheetContent.ContentPriority.LOW);
        when(currentContent.canBeSuppressed(newContent)).thenReturn(true);
        when(currentContent.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());

        when(mBottomSheet.getCurrentSheetContent()).thenReturn(currentContent);
        when(mBottomSheet.isSheetOpen()).thenReturn(true);

        boolean result = mController.requestShowContent(newContent, /* animate= */ true);

        assertTrue("Request should return true as content can be suppressed", result);
        verify(mBottomSheet).setSheetState(SheetState.HIDDEN, true);
    }

    @Test
    public void testRequestShowContent_currentHigh_newLow_canBeSuppressed_returnsTrue() {
        mController.runSheetInitializerForTesting();

        BottomSheetContent newContent = mock(BottomSheetContent.class);
        when(newContent.getPriority()).thenReturn(BottomSheetContent.ContentPriority.LOW);
        BottomSheetContent currentContent = mock(BottomSheetContent.class);
        when(currentContent.getPriority()).thenReturn(BottomSheetContent.ContentPriority.HIGH);
        when(currentContent.canBeSuppressed(newContent)).thenReturn(true);
        when(currentContent.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());

        when(mBottomSheet.getCurrentSheetContent()).thenReturn(currentContent);
        when(mBottomSheet.isSheetOpen()).thenReturn(true);

        boolean result = mController.requestShowContent(newContent, /* animate= */ true);

        assertTrue("Request should return true as content can be suppressed", result);
        verify(mBottomSheet).setSheetState(SheetState.HIDDEN, true);
    }

    @Test
    public void testRequestShowContent_currentHigh_newHigh_canBeSuppressed_returnsTrue() {
        mController.runSheetInitializerForTesting();

        BottomSheetContent newContent = mock(BottomSheetContent.class);
        when(newContent.getPriority()).thenReturn(BottomSheetContent.ContentPriority.HIGH);
        BottomSheetContent currentContent = mock(BottomSheetContent.class);
        when(currentContent.getPriority()).thenReturn(BottomSheetContent.ContentPriority.HIGH);
        when(currentContent.canBeSuppressed(newContent)).thenReturn(true);
        when(currentContent.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());

        when(mBottomSheet.getCurrentSheetContent()).thenReturn(currentContent);
        when(mBottomSheet.isSheetOpen()).thenReturn(true);

        boolean result = mController.requestShowContent(newContent, /* animate= */ true);

        assertTrue("Request should return true as high priority content can be suppressed", result);
        verify(mBottomSheet).setSheetState(SheetState.HIDDEN, true);
    }

    @Test
    public void testRequestShowContent_newCobrowse_closesCurrentSheet() {
        mController.runSheetInitializerForTesting();

        BottomSheetContent newContent = mock(BottomSheetContent.class);
        when(newContent.getPriority()).thenReturn(BottomSheetContent.ContentPriority.COBROWSE);
        BottomSheetContent currentContent = mock(BottomSheetContent.class);
        when(currentContent.getPriority()).thenReturn(BottomSheetContent.ContentPriority.HIGH);
        when(currentContent.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());

        when(mBottomSheet.getCurrentSheetContent()).thenReturn(currentContent);
        when(mBottomSheet.isSheetOpen()).thenReturn(true);

        boolean result = mController.requestShowContent(newContent, /* animate= */ true);

        assertTrue("Request should return true as COBROWSE closes current sheet", result);
        verify(mBottomSheet).setSheetState(SheetState.HIDDEN, true);
    }

    @Test
    public void testRequestShowContent_newCobrowse_alreadySuppressed_clearsContent() {
        mController.runSheetInitializerForTesting();

        BottomSheetContent newContent = mock(BottomSheetContent.class);
        when(newContent.getPriority()).thenReturn(BottomSheetContent.ContentPriority.COBROWSE);
        BottomSheetContent currentContent = mock(BottomSheetContent.class);
        when(currentContent.getPriority()).thenReturn(BottomSheetContent.ContentPriority.LOW);

        when(mBottomSheet.getCurrentSheetContent()).thenReturn(currentContent);

        // Suppress the sheet
        mController.suppressSheet(StateChangeReason.NONE);

        boolean result = mController.requestShowContent(newContent, /* animate= */ true);

        assertFalse("Request should return false as sheet is suppressed", result);
        verify(mBottomSheet).showContent(null);
    }

    @Test
    public void testCobrowseSuppressionAndReturn() {
        mController.runSheetInitializerForTesting();
        verify(mBottomSheet).addObserver(mBottomSheetObserverCaptor.capture());
        when(mBottomSheet.getOpeningState()).thenReturn(BottomSheetController.SheetState.PEEK);

        BottomSheetContent cobrowseContent = mock(BottomSheetContent.class);
        when(cobrowseContent.getPriority()).thenReturn(BottomSheetContent.ContentPriority.COBROWSE);
        when(cobrowseContent.canBeSuppressed(any())).thenReturn(true);
        when(cobrowseContent.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());

        BottomSheetContent highContent = mock(BottomSheetContent.class);
        when(highContent.getPriority()).thenReturn(BottomSheetContent.ContentPriority.HIGH);
        when(highContent.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());

        // 1. Show COBROWSE content.
        mController.requestShowContent(cobrowseContent, /* animate= */ true);
        verify(mBottomSheet).showContent(cobrowseContent);
        when(mBottomSheet.getCurrentSheetContent()).thenReturn(cobrowseContent);

        // 2. Request to show HIGH content.
        mController.requestShowContent(highContent, /* animate= */ true);

        // Verify it tries to hide the current sheet (COBROWSE) to swap.
        verify(mBottomSheet).setSheetState(SheetState.HIDDEN, true);

        // Simulate sheet going to HIDDEN state.
        when(mBottomSheet.getSheetState()).thenReturn(SheetState.HIDDEN);
        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.NONE);

        // 3. Verify HIGH content is shown.
        verify(mBottomSheet).showContent(highContent);
        when(mBottomSheet.getCurrentSheetContent()).thenReturn(highContent);

        // 4. Dismiss HIGH content.
        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.BACK_PRESS);

        // Simulate sheet going to HIDDEN state again after dismissal.
        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.NONE);

        // 5. Verify COBROWSE content returns.
        verify(mBottomSheet, times(2)).showContent(cobrowseContent);
    }

    @Test
    public void testUnsuppressSheet_shouldRestoreStateOnUnsuppress_true() {
        mController.runSheetInitializerForTesting();

        BottomSheetContent content = mock(BottomSheetContent.class);
        when(content.shouldRestoreStateOnUnsuppress()).thenReturn(true);
        when(content.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        when(mBottomSheet.getCurrentSheetContent()).thenReturn(content);
        when(mBottomSheet.getSheetState()).thenReturn(SheetState.HALF);
        when(mBottomSheet.getTargetSheetState()).thenReturn(SheetState.HALF);

        // Suppress the sheet
        int token = mController.suppressSheet(StateChangeReason.NONE);

        // Unsuppress the sheet
        mController.unsuppressSheet(token);

        // Verify that the sheet restored to HALF (its state before suppression)
        verify(mBottomSheet).setSheetState(SheetState.HALF, true);
    }

    @Test
    public void testUnsuppressSheet_shouldRestoreStateOnUnsuppress_false() {
        mController.runSheetInitializerForTesting();

        BottomSheetContent content = mock(BottomSheetContent.class);
        when(content.shouldRestoreStateOnUnsuppress()).thenReturn(false);
        when(content.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        when(mBottomSheet.getCurrentSheetContent()).thenReturn(content);
        when(mBottomSheet.getSheetState()).thenReturn(SheetState.HALF);
        when(mBottomSheet.getTargetSheetState()).thenReturn(SheetState.HALF);
        when(mBottomSheet.getOpeningState()).thenReturn(SheetState.PEEK);

        // Suppress the sheet
        int token = mController.suppressSheet(StateChangeReason.NONE);

        // Unsuppress the sheet
        mController.unsuppressSheet(token);

        // Verify that the sheet collapsed to PEEK (its opening state) instead of HALF
        verify(mBottomSheet).setSheetState(SheetState.PEEK, true);
    }
}
