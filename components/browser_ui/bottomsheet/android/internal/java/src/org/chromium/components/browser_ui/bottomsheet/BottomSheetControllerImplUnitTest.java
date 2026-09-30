// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
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
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** Unit tests for {@link BottomSheetControllerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("DoNotMock") // TODO(567604165): Remove mocking of Views / Activities
public class BottomSheetControllerImplUnitTest {
    private static final int APP_HEADER_HEIGHT = 42;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final OneshotSupplierImpl<ScrimManager> mScrimManagerSupplier =
            new OneshotSupplierImpl<>();
    private final OneshotSupplierImpl<ViewGroup> mRootSupplier = new OneshotSupplierImpl<>();
    private final SettableMonotonicObservableSupplier<Integer> mEdgeToEdgeBottomInsetSupplier =
            ObservableSuppliers.createMonotonic();

    private @Mock ScrimManager mScrimManager;
    private @Mock KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private @Mock ViewGroup mRoot;
    private @Mock DesktopWindowStateManager mDesktopWindowStateManager;
    private @Mock AppHeaderState mAppHeaderState;
    private @Mock BottomSheetCoordinator mBottomSheet;
    private @Mock BottomSheetView mBottomSheetView;
    private @Mock BottomSheetContent mSheetContent;
    private @Mock InsetObserver mInsetObserver;
    private @Captor ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;
    private @Captor ArgumentCaptor<PropertyModel> mScrimPropertyModelCaptor;

    private BottomSheetControllerImpl mController;
    private Window mWindow;

    @Before
    public void setUp() {
        Activity activity = buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mWindow = activity.getWindow();
        when(mRoot.getContext()).thenReturn(activity);
        when(mBottomSheet.getView()).thenReturn(mBottomSheetView);
        when(mBottomSheetView.getContext()).thenReturn(activity);
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
                        mInsetObserver,
                        /* enableLargeFormFactorUi= */ false);
        mController.setBottomSheetForTesting(mBottomSheet);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testAppHeaderStateChange_SheetNotInitialized() {
        // Trigger app header state change before first sheet is triggered.
        when(mAppHeaderState.getAppHeaderHeight()).thenReturn(APP_HEADER_HEIGHT);
        mController.onAppHeaderStateChanged(mAppHeaderState);

        // Simulate sheet initialization, this should kick off
        // BottomSheetCoordinator#onAppHeaderHeightChange().
        mController.runSheetInitializerForTesting();
        verify(mBottomSheet)
                .init(
                        eq(mWindow),
                        eq(mKeyboardVisibilityDelegate),
                        eq(false),
                        eq(mEdgeToEdgeBottomInsetSupplier),
                        eq(APP_HEADER_HEIGHT),
                        eq(0),
                        eq(mInsetObserver),
                        anyBoolean());
    }

    @Test
    public void testIsDesktopUi_FeatureDisabled() {
        DeviceInfo.setIsDesktopForTesting(true);
        // mController is initialized with enableLargeFormFactorUi = false in setUp()
        assertFalse(mController.isLargeFormFactor());
    }

    @Test
    public void testIsDesktopUi_NotDesktop() {
        BottomSheetControllerImpl controller =
                new BottomSheetControllerImpl(
                        mScrimManagerSupplier,
                        mWindow,
                        mKeyboardVisibilityDelegate,
                        mRootSupplier,
                        false,
                        mEdgeToEdgeBottomInsetSupplier,
                        mDesktopWindowStateManager,
                        mInsetObserver,
                        /* enableLargeFormFactorUi= */ true);
        DeviceInfo.setIsDesktopForTesting(false);
        assertFalse(controller.isLargeFormFactor());
    }

    @Test
    public void testIsDesktopUi_Enabled() {
        BottomSheetControllerImpl controller =
                new BottomSheetControllerImpl(
                        mScrimManagerSupplier,
                        mWindow,
                        mKeyboardVisibilityDelegate,
                        mRootSupplier,
                        false,
                        mEdgeToEdgeBottomInsetSupplier,
                        mDesktopWindowStateManager,
                        mInsetObserver,
                        /* enableLargeFormFactorUi= */ true);
        DeviceInfo.setIsDesktopForTesting(true);
        assertTrue(controller.isLargeFormFactor());
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
        verify(mBottomSheet).addObserver(mBottomSheetObserverCaptor.capture());
        doReturn(true).when(mBottomSheet).isSheetOpen();

        // 1. Simulate sheet opened to trigger showScrim and get the callback.
        mBottomSheetObserverCaptor.getValue().onSheetOpened(StateChangeReason.NONE);

        verify(mScrimManager).showScrim(mScrimPropertyModelCaptor.capture());
        var callback =
                mScrimPropertyModelCaptor.getValue().get(ScrimProperties.VISIBILITY_CALLBACK);

        // 2. Trigger callback to hide scrim -> verify Z-elevation is cleared.
        callback.onResult(false);
        verify(mRoot, times(2)).setZ(0.0f);

        // 3. Trigger callback to show scrim -> verify Z-elevation is elevated again.
        callback.onResult(true);
        verify(mRoot, times(2)).setZ(1.0f);
    }

    @Test
    public void testBottomControlsOffset() {
        mController.runSheetInitializerForTesting();
        verify(mBottomSheet).addObserver(mBottomSheetObserverCaptor.capture());
        doReturn(true).when(mBottomSheet).isSheetOpen();

        // 1. Simulate sheet opened to trigger showScrim.
        mBottomSheetObserverCaptor.getValue().onSheetOpened(StateChangeReason.NONE);

        verify(mScrimManager).showScrim(mScrimPropertyModelCaptor.capture());
        var callback =
                mScrimPropertyModelCaptor.getValue().get(ScrimProperties.VISIBILITY_CALLBACK);

        // 2. Set bottom controls offset to 100. Since scrim is visible, bottom margin remains 0.
        mController.setBottomControlsOffset(100);
        verify(mBottomSheet, times(3)).setBottomMargin(0);

        // 3. Trigger callback to hide scrim -> bottom margin should be updated to the offset (100).
        callback.onResult(false);
        verify(mBottomSheet).setBottomMargin(100);
    }

    // Verify that when a sheet content specifies coversBottomControls() as true, it retains
    // a zero bottom margin and elevated Z-axis even when the scrim is hidden.
    @Test
    public void testBottomControlsOffset_coversBottomControls() {
        mController.runSheetInitializerForTesting();
        verify(mBottomSheet).addObserver(mBottomSheetObserverCaptor.capture());
        doReturn(true).when(mBottomSheet).isSheetOpen();
        doReturn(mSheetContent).when(mBottomSheet).getCurrentSheetContent();
        doReturn(true).when(mSheetContent).coversBottomControls();
        doReturn(ObservableSuppliers.alwaysFalse())
                .when(mSheetContent)
                .getBackPressStateChangedSupplier();

        // 1. Simulate sheet opened to trigger showScrim and initial Z-axis / margin adjustment.
        mBottomSheetObserverCaptor.getValue().onSheetOpened(StateChangeReason.NONE);
        verify(mScrimManager).showScrim(mScrimPropertyModelCaptor.capture());
        var callback =
                mScrimPropertyModelCaptor.getValue().get(ScrimProperties.VISIBILITY_CALLBACK);

        // 2. Set bottom controls offset to 100 while scrim is visible -> margin must stay 0.
        mController.setBottomControlsOffset(100);
        verify(mBottomSheet, times(3)).setBottomMargin(0);
        verify(mRoot, times(3)).setZ(1.0f);

        // 3. Hide scrim via callback -> with coversBottomControls() true, bottom margin must
        // remain 0 and Z-axis must stay elevated (1.0f) rather than shifting to offset (100).
        callback.onResult(false);
        verify(mBottomSheet, times(4)).setBottomMargin(0);
        verify(mRoot, times(4)).setZ(1.0f);
    }

    // Verify that when requestShowContent is called with content specifying coversBottomControls()
    // as true, the bottom margin is set to 0 and the Z-axis is elevated immediately when shown.
    @Test
    public void testRequestShowContent_coversBottomControls_adjustsMarginBeforeOpening() {
        mController.runSheetInitializerForTesting();
        mController.setBottomControlsOffset(100);

        BottomSheetContent content = mock(BottomSheetContent.class);
        doReturn(true).when(content).coversBottomControls();
        doReturn(ObservableSuppliers.alwaysFalse())
                .when(content)
                .getBackPressStateChangedSupplier();
        doAnswer(
                        invocation -> {
                            doReturn(content).when(mBottomSheet).getCurrentSheetContent();
                            return null;
                        })
                .when(mBottomSheet)
                .showContent(content);

        mController.requestShowContent(content, /* animate= */ false);

        // Verify that the bottom margin was adjusted to 0 and Z-axis was elevated for this content.
        verify(mBottomSheet).showContent(content);
        verify(mBottomSheet, atLeastOnce()).setBottomMargin(0);
        verify(mRoot, atLeastOnce()).setZ(1.0f);
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
        Supplier<ViewGroup> nullRootSupplier = SupplierUtils.ofNull();
        BottomSheetControllerImpl controllerWithNullRoot =
                new BottomSheetControllerImpl(
                        mScrimManagerSupplier,
                        mWindow,
                        mKeyboardVisibilityDelegate,
                        nullRootSupplier,
                        false,
                        mEdgeToEdgeBottomInsetSupplier,
                        mDesktopWindowStateManager,
                        mInsetObserver,
                        /* enableLargeFormFactorUi= */ false);

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
    public void testHideContent_hiddenDestroyUnsuppresses_showsNextContentBeforeDestroy() {
        mController.runSheetInitializerForTesting();

        BottomSheetContent currentContent = mock(BottomSheetContent.class);
        BottomSheetContent nextContent = mock(BottomSheetContent.class);
        when(currentContent.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        when(nextContent.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        doAnswer(
                        invocation -> {
                            doReturn(invocation.getArgument(0))
                                    .when(mBottomSheet)
                                    .getCurrentSheetContent();
                            return null;
                        })
                .when(mBottomSheet)
                .showContent(any());
        when(mBottomSheet.getCurrentSheetContent()).thenReturn(currentContent);
        when(mBottomSheet.getOpeningState()).thenReturn(SheetState.PEEK);
        when(mBottomSheet.getSheetState()).thenReturn(SheetState.HIDDEN);

        int suppressionToken = mController.suppressSheet(StateChangeReason.NONE);
        mController.requestShowContent(nextContent, /* animate= */ true);
        doAnswer(
                        invocation -> {
                            mController.unsuppressSheet(suppressionToken);
                            return null;
                        })
                .when(currentContent)
                .destroy();

        mController.hideContent(currentContent, /* animate= */ true);

        InOrder inOrder = inOrder(mBottomSheet, currentContent);
        inOrder.verify(mBottomSheet).showContent(nextContent);
        inOrder.verify(currentContent).destroy();
        verify(currentContent, never()).shouldRestoreStateOnUnsuppress();
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

    @Test
    public void testScopedScrimVisibilityCallback() {
        mController.runSheetInitializerForTesting();
        verify(mBottomSheet).addObserver(mBottomSheetObserverCaptor.capture());
        doReturn(true).when(mBottomSheet).isSheetOpen();

        // 1. Simulate bottom sheet opening.
        mBottomSheetObserverCaptor.getValue().onSheetOpened(StateChangeReason.NONE);

        // 2. Capture the PropertyModel passed to showScrim().
        verify(mScrimManager).showScrim(mScrimPropertyModelCaptor.capture());
        PropertyModel capturedModel = mScrimPropertyModelCaptor.getValue();

        // 3. Verify that the visibility callback is registered.
        var visibilityCallback = capturedModel.get(ScrimProperties.VISIBILITY_CALLBACK);
        assertTrue(
                "Visibility callback should be attached to the PropertyModel",
                visibilityCallback != null);

        // 4. Verify that the synchronous callback trigger set Z to 1.0f initially.
        verify(mRoot).setZ(1.0f);

        // 5. Trigger the callback with false -> verify Z-elevation is cleared.
        visibilityCallback.onResult(false);
        verify(mRoot, times(2)).setZ(0.0f);

        // 6. Trigger the callback with true -> verify Z-elevation is elevated again.
        visibilityCallback.onResult(true);
        verify(mRoot, times(2)).setZ(1.0f);
    }

    @Test
    public void testCustomScrimLifecycleBehavior() {
        mController.runSheetInitializerForTesting();
        verify(mBottomSheet).addObserver(mBottomSheetObserverCaptor.capture());
        doReturn(true).when(mBottomSheet).isSheetOpen();

        // 1. Mock the current sheet content to have a custom scrim lifecycle.
        doReturn(true).when(mSheetContent).hasCustomScrimLifecycle();
        doReturn(mSheetContent).when(mBottomSheet).getCurrentSheetContent();
        when(mSheetContent.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());

        // 2. Simulate bottom sheet opening.
        mBottomSheetObserverCaptor.getValue().onSheetOpened(StateChangeReason.NONE);

        // 3. Verify that showScrim was NOT called on ScrimManager.
        verify(mScrimManager, never()).showScrim(any());

        // 4. Verify that Z-elevation remains 0.0f (not elevated to 1.0f automatically).
        verify(mRoot, never()).setZ(1.0f);
    }

    @Test
    public void testSuppressSheet_doesNotCaptureIfTargetStateIsHidden() {
        mController.runSheetInitializerForTesting();

        BottomSheetContent currentContent = mock(BottomSheetContent.class);
        when(currentContent.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());

        when(mBottomSheet.getCurrentSheetContent()).thenReturn(currentContent);
        when(mBottomSheet.getTargetSheetState()).thenReturn(SheetState.HIDDEN);

        int token = mController.suppressSheet(StateChangeReason.NONE);

        verify(mBottomSheet).setSheetState(SheetState.HIDDEN, false, StateChangeReason.NONE);

        mController.unsuppressSheet(token);
        verify(mBottomSheet, never()).showContent(currentContent);
    }

    @Test
    public void testRequestShowContent_doesNotQueueIfTargetStateIsHidden() {
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
        when(mBottomSheet.getTargetSheetState()).thenReturn(SheetState.HIDDEN);

        boolean result = mController.requestShowContent(newContent, /* animate= */ true);

        assertTrue("Request should return true", result);
        verify(mBottomSheet).setSheetState(SheetState.HIDDEN, true);

        // Verify the dying content was not queued by ensuring it doesn't restore when the new
        // content is hidden.
        mController.hideContent(newContent, false, StateChangeReason.NONE);
        verify(mBottomSheet, never()).showContent(currentContent);
    }

    @Test
    public void testHideContent_purgesFromSuppressionCache() {
        mController.runSheetInitializerForTesting();

        BottomSheetContent currentContent = mock(BottomSheetContent.class);
        when(currentContent.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());

        when(mBottomSheet.getCurrentSheetContent()).thenReturn(currentContent);
        when(mBottomSheet.getTargetSheetState()).thenReturn(SheetState.PEEK);

        int token = mController.suppressSheet(StateChangeReason.NONE);
        mController.hideContent(currentContent, false, StateChangeReason.NONE);
        mController.unsuppressSheet(token);

        verify(mBottomSheet, never()).showContent(currentContent);
    }

    @Test
    public void testMetrics_ShowNextContent_RecordsShownHistogram() {
        mController.runSheetInitializerForTesting();

        BottomSheetContent content = mock(BottomSheetContent.class);
        when(content.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        when(mBottomSheet.getSheetState()).thenReturn(SheetState.HIDDEN);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Android.BottomSheet.Shown", true)
                        .build();

        mController.requestShowContent(content, /* animate= */ false);

        watcher.assertExpected();
    }

    @Test
    public void testMetrics_SheetHidden_RecordsClosedReason() {
        mController.runSheetInitializerForTesting();
        verify(mBottomSheet).addObserver(mBottomSheetObserverCaptor.capture());

        BottomSheetContent content = mock(BottomSheetContent.class);
        when(content.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        when(mBottomSheet.getCurrentSheetContent()).thenReturn(content);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.BottomSheet.Closed", StateChangeReason.BACK_PRESS)
                        .build();

        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.BACK_PRESS);

        watcher.assertExpected();
    }

    @Test
    public void testGetMaxSheetHeight_Uninitialized() {
        assertEquals(0, mController.getMaxSheetHeight());
    }

    @Test
    public void testGetMaxSheetHeight_Initialized() {
        mController.runSheetInitializerForTesting();
        doReturn(500).when(mBottomSheet).getMaxSheetHeight();
        assertEquals(500, mController.getMaxSheetHeight());
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_TYPES)
    public void testShowContent_HighPrioritySheetPreemptsAndRestoresLowerPrioritySheet() {
        mController.runSheetInitializerForTesting();
        verify(mBottomSheet).addObserver(mBottomSheetObserverCaptor.capture());
        when(mBottomSheet.getOpeningState()).thenReturn(SheetState.PEEK);

        BottomSheetContent contentA = mock(BottomSheetContent.class);
        BottomSheetType typeA = new BottomSheetType.Builder().setSuppressible(true).build();
        when(contentA.getSheetType()).thenReturn(typeA);
        when(contentA.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());

        BottomSheetContent contentB = mock(BottomSheetContent.class);
        BottomSheetType typeB =
                new BottomSheetType.Builder().setUserInitiated(true).setModal(true).build();
        when(contentB.getSheetType()).thenReturn(typeB);
        when(contentB.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());

        // 1. Show suppressible sheet A.
        boolean shownA = mController.requestShowContent(contentA, /* animate= */ true);
        assertTrue("Sheet A should be shown", shownA);
        when(mBottomSheet.getCurrentSheetContent()).thenReturn(contentA);
        when(mBottomSheet.isSheetOpen()).thenReturn(true);

        // 2. Incoming high-priority modal sheet B preempts sheet A.
        boolean preempted = mController.requestShowContent(contentB, /* animate= */ true);
        assertTrue("Sheet B should preempt sheet A", preempted);
        verify(mBottomSheet).setSheetState(SheetState.HIDDEN, true);

        // Simulate sheet A going to HIDDEN state.
        when(mBottomSheet.getSheetState()).thenReturn(SheetState.HIDDEN);
        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.NONE);
        verify(contentA, never()).destroy();
        when(mBottomSheet.getCurrentSheetContent()).thenReturn(contentB);

        // 3. Close sheet B.
        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.BACK_PRESS);
        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.NONE);
        verify(contentB).destroy();
        verify(contentA, never()).destroy();

        // 4. Sheet A was kept aside while B was shown, and comes back after B closes.
        InOrder inOrder = inOrder(mBottomSheet);
        inOrder.verify(mBottomSheet).showContent(contentA);
        inOrder.verify(mBottomSheet).showContent(contentB);
        inOrder.verify(mBottomSheet).showContent(contentA);
    }

    @Test
    @EnableFeatures(BottomSheetFeatureMap.BOTTOM_SHEET_TYPES)
    public void testShowContent_IncomingPersistentSheetDiscardsPreemptedSheet() {
        mController.runSheetInitializerForTesting();
        verify(mBottomSheet).addObserver(mBottomSheetObserverCaptor.capture());
        when(mBottomSheet.getOpeningState()).thenReturn(SheetState.PEEK);

        // Sheet A is an ordinary sheet that can be pushed aside.
        BottomSheetContent contentA = mock(BottomSheetContent.class);
        BottomSheetType typeA = new BottomSheetType.Builder().setSuppressible(true).build();
        when(contentA.getSheetType()).thenReturn(typeA);
        when(contentA.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());

        // Sheet B is persistent. A persistent sheet does not keep the sheet it replaces.
        BottomSheetContent contentB = mock(BottomSheetContent.class);
        BottomSheetType typeB =
                new BottomSheetType.Builder()
                        .setUserInitiated(true)
                        .setPersistent(true)
                        .setModal(true)
                        .build();
        when(contentB.getSheetType()).thenReturn(typeB);
        when(contentB.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());

        // 1. Show sheet A.
        boolean shownA = mController.requestShowContent(contentA, /* animate= */ true);
        assertTrue("Sheet A should be shown", shownA);
        when(mBottomSheet.getCurrentSheetContent()).thenReturn(contentA);
        when(mBottomSheet.isSheetOpen()).thenReturn(true);

        // 2. Incoming persistent sheet B preempts sheet A.
        boolean preempted = mController.requestShowContent(contentB, /* animate= */ true);
        assertTrue("Sheet B should preempt sheet A", preempted);
        verify(mBottomSheet).setSheetState(SheetState.HIDDEN, true);

        // Simulate sheet A going to HIDDEN state.
        when(mBottomSheet.getSheetState()).thenReturn(SheetState.HIDDEN);
        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.NONE);
        verify(contentA).destroy();
        when(mBottomSheet.getCurrentSheetContent()).thenReturn(contentB);

        // 3. Close sheet B.
        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.BACK_PRESS);
        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.NONE);

        // 4. Sheet A is not brought back after B closes; the sheet is left empty.
        InOrder inOrder = inOrder(mBottomSheet);
        inOrder.verify(mBottomSheet).showContent(contentA);
        inOrder.verify(mBottomSheet).showContent(contentB);
        inOrder.verify(mBottomSheet).showContent(null);

        // Ensure contentA wasn't shown a second time
        verify(mBottomSheet, times(1)).showContent(contentA);
    }

    @Test
    public void testBackPress_WhenContentHandlesBack_CallsContentOnBackPressed() {
        mController.runSheetInitializerForTesting();

        BottomSheetContent content = mock(BottomSheetContent.class);
        when(content.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysTrue());
        when(mBottomSheet.getCurrentSheetContent()).thenReturn(content);

        BackPressHandler backPressHandler = mController.getBottomSheetBackPressHandler();
        int result = backPressHandler.handleBackPress();

        assertEquals(
                "Back press should return SUCCESS when content handles back press",
                BackPressResult.SUCCESS,
                result);
        verify(content).onBackPressed();
        verify(mBottomSheet, never()).setSheetState(anyInt(), anyBoolean(), anyInt());
        verify(mBottomSheet, never()).setSheetState(anyInt(), anyBoolean());
    }

    @Test
    public void testBackPress_WhenContentDoesNotHandleBack_CollapsesSheet() {
        mController.runSheetInitializerForTesting();

        BottomSheetContent content = mock(BottomSheetContent.class);
        when(content.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        when(mBottomSheet.getCurrentSheetContent()).thenReturn(content);
        // The opening and current states differ from the collapse target, so only the lowest
        // swipable state can explain where the sheet goes.
        when(mBottomSheet.getOpeningState()).thenReturn(SheetState.HALF);
        when(mBottomSheet.getSheetState()).thenReturn(SheetState.FULL);
        BackPressHandler backPressHandler = mController.getBottomSheetBackPressHandler();

        when(mBottomSheet.getMinSwipableSheetState()).thenReturn(SheetState.PEEK);
        assertEquals(
                "Back press should return SUCCESS when collapsing the sheet to PEEK",
                BackPressResult.SUCCESS,
                backPressHandler.handleBackPress());
        verify(mBottomSheet).setSheetState(SheetState.PEEK, true, StateChangeReason.BACK_PRESS);

        when(mBottomSheet.getMinSwipableSheetState()).thenReturn(SheetState.HIDDEN);
        assertEquals(
                "Back press should return SUCCESS when collapsing the sheet to HIDDEN",
                BackPressResult.SUCCESS,
                backPressHandler.handleBackPress());
        verify(mBottomSheet).setSheetState(SheetState.HIDDEN, true, StateChangeReason.BACK_PRESS);

        verify(content, never()).onBackPressed();
    }

    @Test
    public void testBackPressSupplier_UpdatesWhenSheetStateOrSuppressionChanges() {
        mController.runSheetInitializerForTesting();
        verify(mBottomSheet).addObserver(mBottomSheetObserverCaptor.capture());

        NonNullObservableSupplier<Boolean> backPressSupplier =
                mController.getBottomSheetBackPressHandler().getHandleBackPressChangedSupplier();
        assertFalse("Initially false before any content is shown", backPressSupplier.get());

        BottomSheetContent content = mock(BottomSheetContent.class);
        when(content.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        when(content.shouldRestoreStateOnUnsuppress()).thenReturn(true);
        when(mBottomSheet.getCurrentSheetContent()).thenReturn(content);
        when(mBottomSheet.isSheetOpen()).thenReturn(true);
        when(mBottomSheet.getTargetSheetState()).thenReturn(SheetState.HALF);

        // 1. Simulate sheet open -> supplier flips to true.
        mBottomSheetObserverCaptor.getValue().onSheetOpened(StateChangeReason.NONE);
        assertTrue(
                "Supplier should be true when sheet is open and unsuppressed",
                backPressSupplier.get());

        // 2. Suppress sheet with token -> supplier flips to false.
        int token = mController.suppressSheet(StateChangeReason.NONE);
        assertFalse(
                "Supplier should be false when sheet is suppressed with token",
                backPressSupplier.get());

        // 3. Unsuppress sheet -> supplier flips back to true.
        mController.unsuppressSheet(token);
        assertTrue("Supplier should be true when sheet is unsuppressed", backPressSupplier.get());

        // 4. Close sheet -> supplier flips to false.
        when(mBottomSheet.isSheetOpen()).thenReturn(false);
        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.BACK_PRESS);
        assertFalse("Supplier should be false when sheet is closed", backPressSupplier.get());
    }

    @Test
    public void testSuppressionTokens_SheetRemainsHiddenUntilAllTokensReleased() {
        mController.runSheetInitializerForTesting();
        verify(mBottomSheet).addObserver(mBottomSheetObserverCaptor.capture());

        BottomSheetContent content = mock(BottomSheetContent.class);
        when(content.shouldRestoreStateOnUnsuppress()).thenReturn(true);
        when(content.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        when(mBottomSheet.getCurrentSheetContent()).thenReturn(content);
        when(mBottomSheet.isSheetOpen()).thenReturn(true);
        when(mBottomSheet.getTargetSheetState()).thenReturn(SheetState.HALF);

        NonNullObservableSupplier<Boolean> backPressSupplier =
                mController.getBottomSheetBackPressHandler().getHandleBackPressChangedSupplier();
        mBottomSheetObserverCaptor.getValue().onSheetOpened(StateChangeReason.NONE);
        assertTrue("An open sheet should handle back presses", backPressSupplier.get());

        // 1. The first suppression hides the sheet.
        int token1 = mController.suppressSheet(StateChangeReason.NONE);
        verify(mBottomSheet).setSheetState(SheetState.HIDDEN, false, StateChangeReason.NONE);
        assertFalse("A suppressed sheet should not handle back presses", backPressSupplier.get());

        // The sheet is now hidden and not moving. If a second suppression saved the state
        // again, it would save HIDDEN instead of HALF.
        when(mBottomSheet.getTargetSheetState()).thenReturn(SheetState.NONE);
        when(mBottomSheet.getSheetState()).thenReturn(SheetState.HIDDEN);

        // 2. A second suppression while the sheet is already hidden.
        int token2 = mController.suppressSheet(StateChangeReason.NONE);

        // 3. Releasing the first token keeps the sheet hidden because token 2 is still held.
        mController.unsuppressSheet(token1);
        assertFalse(
                "The sheet should stay suppressed while another token is held",
                backPressSupplier.get());
        verify(mBottomSheet, never()).setSheetState(eq(SheetState.HALF), anyBoolean());

        // 4. Releasing the last token restores the state from before the first suppression.
        mController.unsuppressSheet(token2);
        assertTrue(
                "The sheet should handle back presses again once all tokens are released",
                backPressSupplier.get());
        verify(mBottomSheet).setSheetState(SheetState.HALF, true);
    }

    @Test
    public void testLegacyBackPress_HandlesOrCollapses() {
        mController.runSheetInitializerForTesting();

        BottomSheetContent content = mock(BottomSheetContent.class);
        when(content.getBackPressStateChangedSupplier())
                .thenReturn(ObservableSuppliers.alwaysFalse());
        when(mBottomSheet.getCurrentSheetContent()).thenReturn(content);
        when(mBottomSheet.isSheetOpen()).thenReturn(true);
        // The opening and current states differ from the collapse target, so only the lowest
        // swipable state can explain where the sheet goes.
        when(mBottomSheet.getOpeningState()).thenReturn(SheetState.HALF);
        when(mBottomSheet.getSheetState()).thenReturn(SheetState.FULL);
        when(mBottomSheet.getMinSwipableSheetState()).thenReturn(SheetState.PEEK);

        // 1. Content handles back press -> returns true without changing sheet state.
        when(content.handleBackPress()).thenReturn(true);
        assertTrue(
                "handleBackPress should return true when content handles back press",
                mController.handleBackPress());
        verify(content).handleBackPress();
        verify(mBottomSheet, never()).setSheetState(anyInt(), anyBoolean(), anyInt());
        verify(mBottomSheet, never()).setSheetState(anyInt(), anyBoolean());

        // 2. Content does not handle back press -> collapses to the lowest swipable state.
        when(content.handleBackPress()).thenReturn(false);
        assertTrue(
                "handleBackPress should return true when collapsing the sheet to PEEK",
                mController.handleBackPress());
        verify(mBottomSheet).setSheetState(SheetState.PEEK, true, StateChangeReason.BACK_PRESS);

        when(mBottomSheet.getMinSwipableSheetState()).thenReturn(SheetState.HIDDEN);
        assertTrue(
                "handleBackPress should return true when collapsing the sheet to HIDDEN",
                mController.handleBackPress());
        verify(mBottomSheet).setSheetState(SheetState.HIDDEN, true, StateChangeReason.BACK_PRESS);

        // 3. Suppressed sheet does not handle back press.
        int token = mController.suppressSheet(StateChangeReason.NONE);
        assertFalse(
                "handleBackPress should return false when sheet is suppressed",
                mController.handleBackPress());
        mController.unsuppressSheet(token);

        // 4. Sheet is not open -> returns false.
        when(mBottomSheet.isSheetOpen()).thenReturn(false);
        assertFalse(
                "handleBackPress should return false when sheet is not open",
                mController.handleBackPress());
    }
}
