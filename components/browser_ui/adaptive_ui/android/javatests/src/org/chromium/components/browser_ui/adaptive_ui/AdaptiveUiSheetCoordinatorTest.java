// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.activity.OnBackPressedCallback;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowInstrumentation;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties.CloseButtonVisibility;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties.Delegate;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties.SecondaryButtonBehavior;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetType;
import org.chromium.components.browser_ui.bottomsheet.UserCriticalFeature;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link AdaptiveUiSheetCoordinator} and {@link AdaptiveUiSheetViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AdaptiveUiSheetCoordinatorTest {
    private static final int TALL_CONTENT_HEIGHT_PX = 3000;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private BottomSheetContent mOtherSheetContent;
    @Mock private Delegate mDelegate;

    private Activity mActivity;
    private boolean mInitialInTouchMode;
    private int mShownCount;
    private final List<Integer> mSheetDismissals = new ArrayList<>();

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mInitialInTouchMode = mActivity.getWindow().getDecorView().isInTouchMode();
    }

    @After
    public void tearDown() {
        ShadowInstrumentation.getInstrumentation().setInTouchMode(mInitialInTouchMode);
        mActivity.finish();
    }

    private AdaptiveUiSheetCoordinator createContent(PropertyModel model) {
        return createContent(model, AdaptiveUiScope.TRANSIENT);
    }

    private AdaptiveUiSheetCoordinator createContent(
            PropertyModel model, @AdaptiveUiScope int scope) {
        return new AdaptiveUiSheetCoordinator(
                mActivity,
                model,
                scope,
                mBottomSheetController,
                () -> mShownCount++,
                mSheetDismissals::add);
    }

    private static PropertyModel.Builder newModelBuilder() {
        return new PropertyModel.Builder(AdaptiveUiProperties.ALL_KEYS);
    }

    private static <T extends View> T find(AdaptiveUiSheetCoordinator content, int id) {
        T view = content.getContentView().findViewById(id);
        assertNotNull(view);
        return view;
    }

    private static void assertAllChildrenGone(ViewGroup group) {
        for (int i = 0; i < group.getChildCount(); i++) {
            assertEquals(
                    "Child " + i + " of " + group + " should be GONE",
                    View.GONE,
                    group.getChildAt(i).getVisibility());
        }
    }

    // ---------------------------------------------------------------------------------------
    // Sheet heights.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testFullInitialState_wrapsContentWithoutHalfState() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());

        assertEquals(HeightMode.DISABLED, content.getHalfHeightRatio(), 0f);
        assertEquals(HeightMode.WRAP_CONTENT, content.getFullHeightRatio(), 0f);
    }

    @Test
    public void testFullInitialState_ignoresCustomHalfRatio() {
        AdaptiveUiSheetCoordinator content =
                createContent(
                        newModelBuilder()
                                .with(AdaptiveUiProperties.SHEET_HALF_HEIGHT_RATIO, 0.5f)
                                .build());

        assertEquals(HeightMode.DISABLED, content.getHalfHeightRatio(), 0f);
    }

    @Test
    public void testExplicitFullInitialState_matchesDefault() {
        AdaptiveUiSheetCoordinator content =
                createContent(
                        newModelBuilder()
                                .with(AdaptiveUiProperties.SHEET_INITIAL_STATE, SheetState.FULL)
                                .with(AdaptiveUiProperties.SHEET_HALF_HEIGHT_RATIO, 0.5f)
                                .build());

        assertEquals(HeightMode.DISABLED, content.getHalfHeightRatio(), 0f);
        assertEquals(HeightMode.WRAP_CONTENT, content.getFullHeightRatio(), 0f);
    }

    @Test
    public void testHalfInitialState_enablesHalfState() {
        AdaptiveUiSheetCoordinator content =
                createContent(
                        newModelBuilder()
                                .with(AdaptiveUiProperties.SHEET_INITIAL_STATE, SheetState.HALF)
                                .build());

        assertEquals(HeightMode.DEFAULT, content.getHalfHeightRatio(), 0f);
        assertEquals(HeightMode.RESIZE_CONTENT, content.getFullHeightRatio(), 0f);
        assertEquals(
                BottomSheetContent.MAX_HEIGHT_RATIO, content.getMaxResizeContentHeightRatio(), 0f);
    }

    @Test
    public void testHalfInitialState_usesCustomRatios() {
        AdaptiveUiSheetCoordinator content =
                createContent(
                        newModelBuilder()
                                .with(AdaptiveUiProperties.SHEET_HALF_HEIGHT_RATIO, 0.5f)
                                .with(AdaptiveUiProperties.SHEET_FULL_HEIGHT_RATIO, 0.9f)
                                .with(AdaptiveUiProperties.SHEET_INITIAL_STATE, SheetState.HALF)
                                .build());

        assertEquals(0.5f, content.getHalfHeightRatio(), 0f);
        assertEquals(HeightMode.RESIZE_CONTENT, content.getFullHeightRatio(), 0f);
        assertEquals(0.9f, content.getMaxResizeContentHeightRatio(), 0f);
    }

    @Test
    public void testHalfInitialState_fullRatioBelowDefaultHalfAsserts() {
        AdaptiveUiSheetCoordinator content =
                createContent(
                        newModelBuilder()
                                .with(AdaptiveUiProperties.SHEET_FULL_HEIGHT_RATIO, 0.5f)
                                .with(AdaptiveUiProperties.SHEET_INITIAL_STATE, SheetState.HALF)
                                .build());

        assertThrows(AssertionError.class, content::getHalfHeightRatio);
    }

    // ---------------------------------------------------------------------------------------
    // Reflective coverage.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testAllKeysAreCoveredByBinderOrNonVisualSet() {
        PropertyModel model = newModelBuilder().build();
        View view = createContent(model).getContentView();
        for (PropertyKey key : AdaptiveUiProperties.ALL_KEYS) {
            if (AdaptiveUiSheetViewBinder.NON_VISUAL_SHEET_KEYS.contains(key)) continue;
            // bind() asserts when it encounters a key that is neither bound nor non-visual, so
            // reaching the end without an AssertionError means the key is bound.
            AdaptiveUiSheetViewBinder.bind(model, view, key);
        }
    }

    @Test
    public void testNonVisualSheetKeysAreExpectedSet() {
        Set<PropertyKey> expected =
                Set.of(
                        AdaptiveUiProperties.DELEGATE,
                        AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION,
                        AdaptiveUiProperties.SHEET_OPENED_ACCESSIBILITY_STRING_ID,
                        AdaptiveUiProperties.SHEET_CLOSED_ACCESSIBILITY_STRING_ID,
                        AdaptiveUiProperties.SHEET_INITIAL_STATE,
                        AdaptiveUiProperties.SHEET_USER_INITIATED,
                        AdaptiveUiProperties.SHEET_USER_CRITICAL,
                        AdaptiveUiProperties.SHEET_HALF_HEIGHT_RATIO,
                        AdaptiveUiProperties.SHEET_FULL_HEIGHT_RATIO,
                        AdaptiveUiProperties.SHOW_SHEET_HANDLEBAR,
                        AdaptiveUiProperties.BACK_PRESS_HANDLER,
                        AdaptiveUiProperties.CUSTOM_VIEW_SCROLLS_ITSELF,
                        AdaptiveUiProperties.GESTURE_DISMISSAL_DISABLED,
                        AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN,
                        AdaptiveUiProperties.SECONDARY_BUTTON_BEHAVIOR,
                        AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS);
        assertEquals(expected, AdaptiveUiSheetViewBinder.NON_VISUAL_SHEET_KEYS);
    }

    // ---------------------------------------------------------------------------------------
    // Button tap and touch protection.
    // ---------------------------------------------------------------------------------------

    private static MotionEvent obtainTouchEvent(long eventTime, int action, int flags) {
        MotionEvent.PointerProperties props = new MotionEvent.PointerProperties();
        props.id = 0;
        MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        coords.x = 1.0f;
        coords.y = 1.0f;
        return MotionEvent.obtain(
                /* downTime= */ eventTime,
                eventTime,
                action,
                /* pointerCount= */ 1,
                new MotionEvent.PointerProperties[] {props},
                new MotionEvent.PointerCoords[] {coords},
                /* metaState= */ 0,
                /* buttonState= */ 0,
                /* xPrecision= */ 1.0f,
                /* yPrecision= */ 1.0f,
                /* deviceId= */ 0,
                /* edgeFlags= */ 0,
                InputDevice.SOURCE_CLASS_POINTER,
                flags);
    }

    @Test
    public void testButtonTapProtection_DefaultOffAllowsImmediateClicks() {
        Delegate primaryDelegate = mock(Delegate.class);
        PropertyModel primaryModel =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.DELEGATE, primaryDelegate)
                        .build();
        assertEquals(0L, primaryModel.get(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS));
        AdaptiveUiSheetCoordinator primaryContent = createContent(primaryModel);
        BottomSheetObserver primaryObserver = showAndCaptureObserver(primaryContent);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(primaryContent);
        primaryObserver.onSheetContentChanged(primaryContent);

        find(primaryContent, R.id.adaptive_ui_sheet_primary_button).performClick();
        verify(primaryDelegate).onPrimaryButtonClicked();
        verify(mBottomSheetController)
                .hideContent(
                        primaryContent,
                        /* animate= */ true,
                        StateChangeReason.INTERACTION_COMPLETE);
        primaryContent.destroy();

        clearInvocations(mBottomSheetController);

        Delegate secondaryDelegate = mock(Delegate.class);
        PropertyModel secondaryModel =
                newModelBuilder()
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .with(AdaptiveUiProperties.DELEGATE, secondaryDelegate)
                        .build();
        AdaptiveUiSheetCoordinator secondaryContent = createContent(secondaryModel);
        BottomSheetObserver secondaryObserver = showAndCaptureObserver(secondaryContent);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(secondaryContent);
        secondaryObserver.onSheetContentChanged(secondaryContent);

        find(secondaryContent, R.id.adaptive_ui_sheet_secondary_button).performClick();
        verify(secondaryDelegate).onSecondaryButtonClicked();
        verify(mBottomSheetController)
                .hideContent(
                        secondaryContent,
                        /* animate= */ true,
                        StateChangeReason.INTERACTION_COMPLETE);
        secondaryContent.destroy();

        assertEquals(
                List.of(DismissReason.PRIMARY_ACTION, DismissReason.SECONDARY_ACTION),
                mSheetDismissals);
    }

    @Test
    public void testButtonTapProtection_BlocksQueuedAndPrematureTapsAndResetsTimer() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .with(
                                AdaptiveUiProperties.SECONDARY_BUTTON_BEHAVIOR,
                                SecondaryButtonBehavior.ACTION_THEN_STAY_OPEN)
                        .with(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, 500L)
                        .with(AdaptiveUiProperties.DELEGATE, mDelegate)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        BottomSheetObserver observer = showAndCaptureObserver(content);
        View primary = find(content, R.id.adaptive_ui_sheet_primary_button);
        View secondary = find(content, R.id.adaptive_ui_sheet_secondary_button);

        // Clicks before onSheetContentChanged(content) are blocked even if time advances.
        ShadowSystemClock.advanceBy(Duration.ofSeconds(1));
        primary.performClick();
        secondary.performClick();
        verify(mDelegate, never()).onPrimaryButtonClicked();
        verify(mDelegate, never()).onSecondaryButtonClicked();
        verify(mBottomSheetController, never()).hideContent(eq(content), anyBoolean(), anyInt());

        // Sheet becomes current and settles open at t0.
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);
        observer.onSheetContentChanged(content);
        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);

        // Clicks at +0ms and +300ms are ignored and each resets the protection start timestamp.
        primary.performClick();
        verify(mDelegate, never()).onPrimaryButtonClicked();

        ShadowSystemClock.advanceBy(Duration.ofMillis(300));
        primary.performClick();
        verify(mDelegate, never()).onPrimaryButtonClicked();

        // At +600ms from show (300ms after the premature tap, <= 500ms), click is still blocked.
        ShadowSystemClock.advanceBy(Duration.ofMillis(300));
        primary.performClick();
        verify(mDelegate, never()).onPrimaryButtonClicked();

        // Exact inclusive boundary (+500ms since last tap) is also blocked and resets the timer.
        ShadowSystemClock.advanceBy(Duration.ofMillis(500));
        secondary.performClick();
        verify(mDelegate, never()).onSecondaryButtonClicked();

        // Advancing +501ms allows the secondary click and restarts the protection window.
        ShadowSystemClock.advanceBy(Duration.ofMillis(501));
        secondary.performClick();
        verify(mDelegate).onSecondaryButtonClicked();
        primary.performClick();
        verify(mDelegate, never()).onPrimaryButtonClicked();

        // Advancing another +501ms allows the primary click and dismisses the sheet.
        ShadowSystemClock.advanceBy(Duration.ofMillis(501));
        primary.performClick();
        verify(mDelegate).onPrimaryButtonClicked();
        verify(mBottomSheetController)
                .hideContent(content, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
        content.destroy();
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mSheetDismissals);
    }

    @Test
    public void testButtonTapProtection_CloseButtonAndBackPressUnaffected() {
        Delegate closeDelegate = mock(Delegate.class);
        PropertyModel closeModel =
                newModelBuilder()
                        .with(
                                AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY,
                                CloseButtonVisibility.SHOWN)
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, 500L)
                        .with(AdaptiveUiProperties.DELEGATE, closeDelegate)
                        .build();
        AdaptiveUiSheetCoordinator closeContent = createContent(closeModel);
        BottomSheetObserver closeObserver = showAndCaptureObserver(closeContent);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(closeContent);
        closeObserver.onSheetContentChanged(closeContent);
        closeObserver.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);

        find(closeContent, R.id.adaptive_ui_sheet_primary_button).performClick();
        verify(closeDelegate, never()).onPrimaryButtonClicked();

        find(closeContent, R.id.adaptive_ui_sheet_close_button).performClick();
        verify(mBottomSheetController)
                .hideContent(closeContent, /* animate= */ true, StateChangeReason.CLOSE_BUTTON);
        closeContent.destroy();

        clearInvocations(mBottomSheetController);

        Delegate backDelegate = mock(Delegate.class);
        PropertyModel backModel =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, 500L)
                        .with(AdaptiveUiProperties.DELEGATE, backDelegate)
                        .build();
        AdaptiveUiSheetCoordinator backContent = createContent(backModel);
        BottomSheetObserver backObserver = showAndCaptureObserver(backContent);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(backContent);
        backObserver.onSheetContentChanged(backContent);
        backObserver.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);

        find(backContent, R.id.adaptive_ui_sheet_primary_button).performClick();
        verify(backDelegate, never()).onPrimaryButtonClicked();
        verify(closeDelegate, never()).onPrimaryButtonClicked();

        backContent.onBackPressed();
        verify(mBottomSheetController)
                .hideContent(backContent, /* animate= */ true, StateChangeReason.BACK_PRESS);
        backContent.destroy();

        assertEquals(
                List.of(DismissReason.CLOSE_BUTTON, DismissReason.BACK_PRESS), mSheetDismissals);
    }

    @Test
    public void testButtonTapProtection_BlocksDuringSlideInUntilSettled() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, 100L)
                        .with(AdaptiveUiProperties.DELEGATE, mDelegate)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        BottomSheetObserver observer = showAndCaptureObserver(content);
        View primary = find(content, R.id.adaptive_ui_sheet_primary_button);

        // Taps stay blocked during the slide-in, even when it takes longer than the period.
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);
        observer.onSheetContentChanged(content);
        observer.onSheetStateChanged(SheetState.SCROLLING, StateChangeReason.NONE);
        ShadowSystemClock.advanceBy(Duration.ofMillis(200));
        primary.performClick();
        verify(mDelegate, never()).onPrimaryButtonClicked();

        observer.onSheetStateChanged(SheetState.PEEK, StateChangeReason.NONE);
        ShadowSystemClock.advanceBy(Duration.ofMillis(150));
        primary.performClick();
        verify(mDelegate, never()).onPrimaryButtonClicked();

        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);

        // Later drags (FULL -> SCROLLING -> FULL) don't restart the period.
        ShadowSystemClock.advanceBy(Duration.ofMillis(50));
        observer.onSheetStateChanged(SheetState.SCROLLING, StateChangeReason.SWIPE);
        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.SWIPE);

        ShadowSystemClock.advanceBy(Duration.ofMillis(51));
        primary.performClick();
        verify(mDelegate).onPrimaryButtonClicked();
        verify(mBottomSheetController)
                .hideContent(content, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
        content.destroy();
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mSheetDismissals);
    }

    @Test
    public void testButtonTapProtection_ReshowResetsPeriod() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN, true)
                        .with(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, 500L)
                        .with(AdaptiveUiProperties.DELEGATE, mDelegate)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        BottomSheetObserver observer = showAndCaptureObserver(content);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);
        observer.onSheetContentChanged(content);
        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        View primary = find(content, R.id.adaptive_ui_sheet_primary_button);

        ShadowSystemClock.advanceBy(Duration.ofMillis(501));
        primary.performClick();
        verify(mDelegate).onPrimaryButtonClicked();
        ShadowSystemClock.advanceBy(Duration.ofMillis(501));

        // Swapping into an already-open sheet (FULL) arms immediately without a state change.
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mOtherSheetContent);
        observer.onSheetContentChanged(mOtherSheetContent);
        ShadowSystemClock.advanceBy(Duration.ofSeconds(1));
        primary.performClick();
        verify(mDelegate).onPrimaryButtonClicked();

        when(mBottomSheetController.getSheetState()).thenReturn(SheetState.FULL);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);
        observer.onSheetContentChanged(content);
        primary.performClick();
        verify(mDelegate).onPrimaryButtonClicked();

        ShadowSystemClock.advanceBy(Duration.ofMillis(501));
        primary.performClick();
        verify(mDelegate, times(2)).onPrimaryButtonClicked();
        ShadowSystemClock.advanceBy(Duration.ofMillis(501));

        // Hiding and re-opening while still current (no content swap) stays blocked through
        // SCROLLING and re-arms on the settled state.
        when(mBottomSheetController.getSheetState()).thenReturn(SheetState.HIDDEN);
        observer.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.OMNIBOX_FOCUS);
        ShadowSystemClock.advanceBy(Duration.ofSeconds(1));
        primary.performClick();
        verify(mDelegate, times(2)).onPrimaryButtonClicked();

        observer.onSheetStateChanged(SheetState.NONE, StateChangeReason.NONE);
        primary.performClick();
        verify(mDelegate, times(2)).onPrimaryButtonClicked();

        observer.onSheetStateChanged(SheetState.SCROLLING, StateChangeReason.NONE);
        ShadowSystemClock.advanceBy(Duration.ofSeconds(1));
        primary.performClick();
        verify(mDelegate, times(2)).onPrimaryButtonClicked();

        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        primary.performClick();
        verify(mDelegate, times(2)).onPrimaryButtonClicked();

        ShadowSystemClock.advanceBy(Duration.ofMillis(501));
        primary.performClick();
        verify(mDelegate, times(3)).onPrimaryButtonClicked();
    }

    @Test
    public void testFilterTouchForSecurity_DefaultFalse() {
        PropertyModel model =
                newModelBuilder()
                        .with(
                                AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY,
                                CloseButtonVisibility.SHOWN)
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        assertFalse(model.get(AdaptiveUiProperties.FILTER_TOUCH_FOR_SECURITY));
        assertFalse(
                find(content, R.id.adaptive_ui_sheet_primary_button)
                        .getFilterTouchesWhenObscured());
        assertFalse(
                find(content, R.id.adaptive_ui_sheet_secondary_button)
                        .getFilterTouchesWhenObscured());
        assertFalse(
                find(content, R.id.adaptive_ui_sheet_close_button).getFilterTouchesWhenObscured());
    }

    @Test
    public void testFilterTouchForSecurity_EnabledFiltersObscuredTouchesOnActionButtonsOnly() {
        PropertyModel model =
                newModelBuilder()
                        .with(
                                AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY,
                                CloseButtonVisibility.SHOWN)
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN, true)
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .with(
                                AdaptiveUiProperties.SECONDARY_BUTTON_BEHAVIOR,
                                SecondaryButtonBehavior.ACTION_THEN_STAY_OPEN)
                        .with(AdaptiveUiProperties.FILTER_TOUCH_FOR_SECURITY, true)
                        .with(AdaptiveUiProperties.DELEGATE, mDelegate)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        View root = content.getContentView();
        mActivity.setContentView(root);
        root.measure(
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(600, View.MeasureSpec.EXACTLY));
        root.layout(0, 0, 1000, 600);

        View primary = find(content, R.id.adaptive_ui_sheet_primary_button);
        View secondary = find(content, R.id.adaptive_ui_sheet_secondary_button);
        View close = find(content, R.id.adaptive_ui_sheet_close_button);

        assertTrue(primary.getFilterTouchesWhenObscured());
        assertTrue(secondary.getFilterTouchesWhenObscured());
        assertFalse(close.getFilterTouchesWhenObscured());

        // Partially-obscured touch events are consumed without pressing buttons or firing clicks.
        long now = SystemClock.uptimeMillis();
        MotionEvent obscuredDown =
                obtainTouchEvent(
                        now,
                        MotionEvent.ACTION_DOWN,
                        MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED);
        MotionEvent obscuredUp =
                obtainTouchEvent(
                        now, MotionEvent.ACTION_UP, MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED);
        assertTrue(primary.dispatchTouchEvent(obscuredDown));
        assertFalse(primary.isPressed());
        assertTrue(primary.dispatchTouchEvent(obscuredUp));
        assertTrue(secondary.dispatchTouchEvent(obscuredDown));
        assertFalse(secondary.isPressed());
        assertTrue(secondary.dispatchTouchEvent(obscuredUp));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mDelegate, never()).onPrimaryButtonClicked();
        verify(mDelegate, never()).onSecondaryButtonClicked();

        // Unobscured touch events (flags = 0) are not consumed by the security listener.
        long later = SystemClock.uptimeMillis();
        MotionEvent unobscuredDown =
                obtainTouchEvent(later, MotionEvent.ACTION_DOWN, /* flags= */ 0);
        MotionEvent unobscuredUp = obtainTouchEvent(later, MotionEvent.ACTION_UP, /* flags= */ 0);
        primary.dispatchTouchEvent(unobscuredDown);
        primary.dispatchTouchEvent(unobscuredUp);
        secondary.dispatchTouchEvent(unobscuredDown);
        secondary.dispatchTouchEvent(unobscuredUp);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mDelegate).onPrimaryButtonClicked();
        verify(mDelegate).onSecondaryButtonClicked();

        obscuredDown.recycle();
        obscuredUp.recycle();
        unobscuredDown.recycle();
        unobscuredUp.recycle();
    }

    // ---------------------------------------------------------------------------------------
    // Visibility.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testUnsetKeysLeaveOptionalViewsGone() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());

        assertEquals(View.GONE, find(content, R.id.adaptive_ui_sheet_icon).getVisibility());
        assertEquals(View.GONE, find(content, R.id.adaptive_ui_sheet_title).getVisibility());
        assertEquals(View.GONE, find(content, R.id.adaptive_ui_sheet_description).getVisibility());
        assertEquals(
                View.GONE, find(content, R.id.adaptive_ui_sheet_footer_message).getVisibility());
        assertEquals(View.GONE, find(content, R.id.adaptive_ui_sheet_close_button).getVisibility());
        assertEquals(
                View.GONE,
                find(content, R.id.adaptive_ui_sheet_custom_view_container).getVisibility());
        assertEquals(
                View.GONE, find(content, R.id.adaptive_ui_sheet_primary_button).getVisibility());
        assertEquals(
                View.GONE, find(content, R.id.adaptive_ui_sheet_secondary_button).getVisibility());

        // Header and button bar collapse because all of their children are gone.
        assertEquals(View.GONE, find(content, R.id.adaptive_ui_sheet_header).getVisibility());
        assertAllChildrenGone(find(content, R.id.adaptive_ui_sheet_header_content));
        assertAllChildrenGone(find(content, R.id.adaptive_ui_sheet_button_bar));
        content.getContentView()
                .measure(
                        View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                        View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
        assertEquals(0, find(content, R.id.adaptive_ui_sheet_header).getMeasuredHeight());
        assertEquals(0, find(content, R.id.adaptive_ui_sheet_button_bar).getMeasuredHeight());
    }

    @Test
    public void testHeader_CentersTitleWithCloseButtonWithoutClipping() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.TITLE, "Title")
                        .with(
                                AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY,
                                CloseButtonVisibility.SHOWN)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        View root = content.getContentView();
        root.measure(
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(600, View.MeasureSpec.EXACTLY));
        root.layout(0, 0, 1000, 600);

        View header = find(content, R.id.adaptive_ui_sheet_header);
        View title = find(content, R.id.adaptive_ui_sheet_title);
        View close = find(content, R.id.adaptive_ui_sheet_close_button);

        assertEquals(View.VISIBLE, header.getVisibility());
        assertTrue(topIn(header, title) + title.getHeight() <= header.getHeight());
        // Without an icon, the close button is on the title's line, vertically centered with it.
        int titleCenter = 2 * topIn(header, title) + title.getHeight();
        int closeCenter = 2 * topIn(header, close) + close.getHeight();
        assertTrue(Math.abs(titleCenter - closeCenter) <= 1);
        // The title stays horizontally centered in the sheet even with the close button shown.
        int titleHorizontalCenter = 2 * leftIn(header, title) + title.getWidth();
        assertTrue(Math.abs(titleHorizontalCenter - header.getWidth()) <= 1);
    }

    @Test
    public void testHeader_IconTitleAndCloseButton_CloseAtTopAndTitleCentered() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.HEADER_ICON, new ColorDrawable(Color.RED))
                        .with(AdaptiveUiProperties.TITLE, "Title")
                        .with(
                                AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY,
                                CloseButtonVisibility.SHOWN)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        View root = content.getContentView();
        root.measure(
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(600, View.MeasureSpec.EXACTLY));
        root.layout(0, 0, 1000, 600);

        View header = find(content, R.id.adaptive_ui_sheet_header);
        View icon = find(content, R.id.adaptive_ui_sheet_icon);
        View title = find(content, R.id.adaptive_ui_sheet_title);
        View close = find(content, R.id.adaptive_ui_sheet_close_button);

        // The close button is vertically centered on the icon's line, and the icon stays centered
        // on the full width.
        int iconCenter = 2 * topIn(header, icon) + icon.getHeight();
        int closeCenter = 2 * topIn(header, close) + close.getHeight();
        assertTrue(Math.abs(iconCenter - closeCenter) <= 1);
        int iconHorizontalCenter = 2 * leftIn(header, icon) + icon.getWidth();
        assertTrue(Math.abs(iconHorizontalCenter - header.getWidth()) <= 1);
        assertEquals(
                topIn(header, icon) + icon.getHeight() + getVerticalMargin(), topIn(header, title));
        // With an icon, the title only needs its text height, so the gap stays 16dp.
        assertEquals(0, ((TextView) title).getMinHeight());
        int titleHorizontalCenter = 2 * leftIn(header, title) + title.getWidth();
        assertTrue(Math.abs(titleHorizontalCenter - header.getWidth()) <= 1);

        // Removing the title later also removes the gap below the icon.
        model.set(AdaptiveUiProperties.TITLE, null);
        assertEquals(0, ((ViewGroup.MarginLayoutParams) icon.getLayoutParams()).bottomMargin);
    }

    @Test
    public void testHeader_IconWithoutTitle_AddsNoGapBelowIcon() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.HEADER_ICON, new ColorDrawable(Color.RED))
                        .with(AdaptiveUiProperties.DESCRIPTION, "Description")
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        View root = content.getContentView();
        root.measure(
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(600, View.MeasureSpec.EXACTLY));
        root.layout(0, 0, 1000, 600);

        View header = find(content, R.id.adaptive_ui_sheet_header);
        View icon = find(content, R.id.adaptive_ui_sheet_icon);
        View description = find(content, R.id.adaptive_ui_sheet_description);

        // The header ends right below the icon, and only the description's own top margin follows.
        assertEquals(topIn(header, icon) + icon.getHeight(), header.getHeight());
        assertEquals(header.getBottom() + getVerticalMargin(), topIn(root, description));
    }

    /** Returns the top of {@code view} relative to its ancestor {@code ancestor}. */
    private static int topIn(View ancestor, View view) {
        int top = 0;
        for (View v = view; v != ancestor; v = (View) v.getParent()) top += v.getTop();
        return top;
    }

    /** Returns the left of {@code view} relative to its ancestor {@code ancestor}. */
    private static int leftIn(View ancestor, View view) {
        int left = 0;
        for (View v = view; v != ancestor; v = (View) v.getParent()) left += v.getLeft();
        return left;
    }

    @Test
    public void testGetSheetTypeReflectsModalityAndScope() {
        AdaptiveUiSheetCoordinator windowContent =
                createContent(newModelBuilder().build(), AdaptiveUiScope.WINDOW);
        assertTrue(windowContent.getSheetType().isModal());
        assertTrue(windowContent.getSheetType().isPersistent());
        assertFalse(windowContent.getSheetType().isUserInitiated());
        assertFalse(windowContent.getSheetType().isUserCritical());

        AdaptiveUiSheetCoordinator transientContent =
                createContent(newModelBuilder().build(), AdaptiveUiScope.TRANSIENT);
        assertTrue(transientContent.getSheetType().isModal());
        assertFalse(transientContent.getSheetType().isPersistent());
        assertFalse(transientContent.getSheetType().isUserInitiated());
        assertFalse(transientContent.getSheetType().isUserCritical());
    }

    @Test
    public void testGetSheetTypeReflectsPrecedence() {
        AdaptiveUiSheetCoordinator content =
                createContent(
                        newModelBuilder()
                                .with(AdaptiveUiProperties.SHEET_USER_INITIATED, true)
                                .with(
                                        AdaptiveUiProperties.SHEET_USER_CRITICAL,
                                        UserCriticalFeature.ACCOUNT_PICKER)
                                .build());
        BottomSheetType expected =
                new BottomSheetType.Builder()
                        .setModal(/* isModal= */ true)
                        .setPersistent(false)
                        .setUserInitiated(true)
                        .setUserCritical(UserCriticalFeature.ACCOUNT_PICKER)
                        .build();
        assertEquals(expected, content.getSheetType());
        assertTrue(content.getSheetType().isUserInitiated());
        assertTrue(content.getSheetType().isUserCritical());
    }

    @Test
    public void testSettingVisualKeysMakesViewsVisible() {
        Drawable icon = new ColorDrawable(Color.RED);
        View customView = new View(mActivity);
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.HEADER_ICON, icon)
                        .with(AdaptiveUiProperties.TITLE, "Title")
                        .with(AdaptiveUiProperties.DESCRIPTION, "Description")
                        .with(AdaptiveUiProperties.FOOTER_MESSAGE, "Footer")
                        .with(
                                AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY,
                                CloseButtonVisibility.SHOWN)
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, customView)
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        assertEquals(View.VISIBLE, find(content, R.id.adaptive_ui_sheet_header).getVisibility());

        ImageView iconView = find(content, R.id.adaptive_ui_sheet_icon);
        assertEquals(View.VISIBLE, iconView.getVisibility());
        assertSame(icon, iconView.getDrawable());

        TextView title = find(content, R.id.adaptive_ui_sheet_title);
        assertEquals(View.VISIBLE, title.getVisibility());
        assertEquals("Title", title.getText().toString());

        TextView description = find(content, R.id.adaptive_ui_sheet_description);
        assertEquals(View.VISIBLE, description.getVisibility());
        assertEquals("Description", description.getText().toString());

        TextView footer = find(content, R.id.adaptive_ui_sheet_footer_message);
        assertEquals(View.VISIBLE, footer.getVisibility());
        assertEquals("Footer", footer.getText().toString());

        assertEquals(
                View.VISIBLE, find(content, R.id.adaptive_ui_sheet_close_button).getVisibility());

        ViewGroup container = find(content, R.id.adaptive_ui_sheet_custom_view_container);
        assertEquals(View.VISIBLE, container.getVisibility());
        assertSame(container, customView.getParent());

        TextView primary = find(content, R.id.adaptive_ui_sheet_primary_button);
        assertEquals(View.VISIBLE, primary.getVisibility());
        assertEquals("Primary", primary.getText().toString());

        TextView secondary = find(content, R.id.adaptive_ui_sheet_secondary_button);
        assertEquals(View.VISIBLE, secondary.getVisibility());
        assertEquals("Secondary", secondary.getText().toString());
    }

    @Test
    public void testEmptyTextKeepsViewGone() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.TITLE, "")
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "")
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        assertEquals(View.GONE, find(content, R.id.adaptive_ui_sheet_title).getVisibility());
        assertEquals(
                View.GONE, find(content, R.id.adaptive_ui_sheet_primary_button).getVisibility());
    }

    @Test
    public void testCustomViewIsReparentedFromPreviousParent() {
        FrameLayout oldParent = new FrameLayout(mActivity);
        View customView = new View(mActivity);
        oldParent.addView(customView);
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.CUSTOM_VIEW, customView).build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        assertSame(
                find(content, R.id.adaptive_ui_sheet_custom_view_container),
                customView.getParent());
        assertEquals(0, oldParent.getChildCount());
    }

    @Test
    public void testCustomViewHasNoSideMargin() {
        View customView = new View(mActivity);
        AdaptiveUiSheetCoordinator content =
                createContent(
                        newModelBuilder()
                                .with(AdaptiveUiProperties.DESCRIPTION, "Description")
                                .with(AdaptiveUiProperties.CUSTOM_VIEW, customView)
                                .with(AdaptiveUiProperties.FOOTER_MESSAGE, "Footer")
                                .build());
        int width = 1000;
        View contentView = content.getContentView();
        contentView.measure(
                View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
        contentView.layout(0, 0, width, contentView.getMeasuredHeight());

        // Like the dialog, the sheet leaves side padding to the custom view; text keeps its margin.
        int margin =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.adaptive_ui_sheet_horizontal_margin);
        assertEquals(width, customView.getWidth());
        assertEquals(
                width - 2 * margin, find(content, R.id.adaptive_ui_sheet_description).getWidth());
        assertEquals(
                width - 2 * margin,
                find(content, R.id.adaptive_ui_sheet_footer_message).getWidth());
    }

    private static int getTopMargin(View view) {
        return ((ViewGroup.MarginLayoutParams) view.getLayoutParams()).topMargin;
    }

    private int getVerticalMargin() {
        return mActivity
                .getResources()
                .getDimensionPixelSize(R.dimen.adaptive_ui_sheet_vertical_margin);
    }

    @Test
    public void testCustomView_NoTopMarginInRegularBody() {
        AdaptiveUiSheetCoordinator content =
                createContent(
                        newModelBuilder()
                                .with(AdaptiveUiProperties.CUSTOM_VIEW, new View(mActivity))
                                .build());

        assertEquals(0, getTopMargin(find(content, R.id.adaptive_ui_sheet_custom_view_container)));
    }

    @Test
    public void testCustomView_NoTopMarginInSelfScrollingBody() {
        AdaptiveUiSheetCoordinator content =
                createContent(
                        newModelBuilder()
                                .with(AdaptiveUiProperties.CUSTOM_VIEW, newScrollingBody())
                                .with(AdaptiveUiProperties.CUSTOM_VIEW_SCROLLS_ITSELF, true)
                                .build());

        assertNull(content.getContentView().findViewById(R.id.adaptive_ui_sheet_scroll_view));
        assertEquals(0, getTopMargin(find(content, R.id.adaptive_ui_sheet_custom_view_container)));
    }

    @Test
    public void testPrimaryButton_NoTopMarginDirectlyBelowCustomView() {
        AdaptiveUiSheetCoordinator content =
                createContent(
                        newModelBuilder()
                                .with(AdaptiveUiProperties.CUSTOM_VIEW, new View(mActivity))
                                .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                                .build());

        assertEquals(0, getTopMargin(find(content, R.id.adaptive_ui_sheet_primary_button)));
    }

    @Test
    public void testPrimaryButton_KeepsTopMarginBelowDescriptionOnly() {
        AdaptiveUiSheetCoordinator content =
                createContent(
                        newModelBuilder()
                                .with(AdaptiveUiProperties.DESCRIPTION, "Description")
                                .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                                .build());

        assertEquals(
                getVerticalMargin(),
                getTopMargin(find(content, R.id.adaptive_ui_sheet_primary_button)));
    }

    @Test
    public void testPrimaryButton_KeepsTopMarginWhenFooterFollowsCustomView() {
        AdaptiveUiSheetCoordinator content =
                createContent(
                        newModelBuilder()
                                .with(AdaptiveUiProperties.CUSTOM_VIEW, new View(mActivity))
                                .with(AdaptiveUiProperties.FOOTER_MESSAGE, "Footer")
                                .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                                .build());

        assertEquals(
                getVerticalMargin(),
                getTopMargin(find(content, R.id.adaptive_ui_sheet_primary_button)));
    }

    @Test
    public void testSheetRoot_KeepsBottomPaddingWithCustomView() {
        AdaptiveUiSheetCoordinator content =
                createContent(
                        newModelBuilder()
                                .with(AdaptiveUiProperties.CUSTOM_VIEW, new View(mActivity))
                                .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                                .build());

        assertEquals(getVerticalMargin(), content.getContentView().getPaddingBottom());
    }

    // ---------------------------------------------------------------------------------------
    // Live updates.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testPrimaryButton_UpdatesTopMarginWhenFooterSetAndClearedAfterShow() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, new View(mActivity))
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        View primaryButton = find(content, R.id.adaptive_ui_sheet_primary_button);
        assertEquals(0, getTopMargin(primaryButton));

        model.set(AdaptiveUiProperties.FOOTER_MESSAGE, "Footer");
        assertEquals(getVerticalMargin(), getTopMargin(primaryButton));

        model.set(AdaptiveUiProperties.FOOTER_MESSAGE, null);
        assertEquals(0, getTopMargin(primaryButton));
    }

    @Test
    public void testPrimaryButton_RestoresTopMarginWhenCustomViewRemovedAfterShow() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, new View(mActivity))
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        View primaryButton = find(content, R.id.adaptive_ui_sheet_primary_button);
        assertEquals(0, getTopMargin(primaryButton));

        model.set(AdaptiveUiProperties.CUSTOM_VIEW, null);
        assertEquals(getVerticalMargin(), getTopMargin(primaryButton));
    }

    @Test
    public void testLiveUpdatesOfWritableProperties() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        TextView title = find(content, R.id.adaptive_ui_sheet_title);
        TextView footer = find(content, R.id.adaptive_ui_sheet_footer_message);
        View primary = find(content, R.id.adaptive_ui_sheet_primary_button);
        View secondary = find(content, R.id.adaptive_ui_sheet_secondary_button);

        model.set(AdaptiveUiProperties.TITLE, "Title");
        assertEquals(View.VISIBLE, title.getVisibility());
        assertEquals("Title", title.getText().toString());
        model.set(AdaptiveUiProperties.TITLE, null);
        assertEquals(View.GONE, title.getVisibility());

        model.set(AdaptiveUiProperties.FOOTER_MESSAGE, "Footer");
        assertEquals(View.VISIBLE, footer.getVisibility());
        assertEquals("Footer", footer.getText().toString());
        model.set(AdaptiveUiProperties.FOOTER_MESSAGE, null);
        assertEquals(View.GONE, footer.getVisibility());

        assertTrue(primary.isEnabled());
        model.set(AdaptiveUiProperties.PRIMARY_BUTTON_DISABLED, true);
        assertFalse(primary.isEnabled());
        model.set(AdaptiveUiProperties.PRIMARY_BUTTON_DISABLED, false);
        assertTrue(primary.isEnabled());

        assertTrue(secondary.isEnabled());
        model.set(AdaptiveUiProperties.SECONDARY_BUTTON_DISABLED, true);
        assertFalse(secondary.isEnabled());
        model.set(AdaptiveUiProperties.SECONDARY_BUTTON_DISABLED, false);
        assertTrue(secondary.isEnabled());
    }

    // ---------------------------------------------------------------------------------------
    // Destroy.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testDestroyDetachesChangeProcessorAndCustomView() {
        View customView = new View(mActivity);
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.TITLE, "Title")
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, customView)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        TextView title = find(content, R.id.adaptive_ui_sheet_title);
        ViewGroup container = find(content, R.id.adaptive_ui_sheet_custom_view_container);
        assertSame(container, customView.getParent());

        content.destroy();

        assertNull(customView.getParent());
        assertEquals(0, container.getChildCount());
        model.set(AdaptiveUiProperties.TITLE, "Changed after destroy");
        assertEquals("Title", title.getText().toString());

        // Safe to call more than once.
        content.destroy();
    }

    // ---------------------------------------------------------------------------------------
    // Clicks and back presses.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testPrimaryClick_RunsCallbackAndRequestsDismiss() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.DELEGATE, mDelegate)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        find(content, R.id.adaptive_ui_sheet_primary_button).performClick();

        verify(mDelegate).onPrimaryButtonClicked();
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mSheetDismissals);
    }

    @Test
    public void testSecondaryClick_RunsCallbackAndRequestsDismiss() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .with(AdaptiveUiProperties.DELEGATE, mDelegate)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        find(content, R.id.adaptive_ui_sheet_secondary_button).performClick();

        verify(mDelegate).onSecondaryButtonClicked();
        assertEquals(List.of(DismissReason.SECONDARY_ACTION), mSheetDismissals);
    }

    @Test
    public void testPrimaryClick_KeepsOpen() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN, true)
                        .with(AdaptiveUiProperties.DELEGATE, mDelegate)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        View primary = find(content, R.id.adaptive_ui_sheet_primary_button);
        primary.performClick();
        primary.performClick();

        verify(mDelegate, times(2)).onPrimaryButtonClicked();
        assertTrue(mSheetDismissals.isEmpty());
    }

    @Test
    public void testSecondaryClick_KeepsOpen() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .with(
                                AdaptiveUiProperties.SECONDARY_BUTTON_BEHAVIOR,
                                SecondaryButtonBehavior.ACTION_THEN_STAY_OPEN)
                        .with(AdaptiveUiProperties.DELEGATE, mDelegate)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        find(content, R.id.adaptive_ui_sheet_secondary_button).performClick();

        verify(mDelegate).onSecondaryButtonClicked();
        assertTrue(mSheetDismissals.isEmpty());
    }

    @Test
    public void testClickWithoutCallback_StillRequestsDismiss() {
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary").build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        find(content, R.id.adaptive_ui_sheet_primary_button).performClick();

        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mSheetDismissals);
    }

    @Test
    public void testDismissRequestOnlyReportedOnce() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        find(content, R.id.adaptive_ui_sheet_primary_button).performClick();
        find(content, R.id.adaptive_ui_sheet_secondary_button).performClick();
        content.onBackPressed();

        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mSheetDismissals);
    }

    @Test
    public void testCloseButtonClick_RequestsDismiss() {
        PropertyModel model =
                newModelBuilder()
                        .with(
                                AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY,
                                CloseButtonVisibility.SHOWN)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        find(content, R.id.adaptive_ui_sheet_close_button).performClick();

        assertEquals(List.of(DismissReason.CLOSE_BUTTON), mSheetDismissals);
    }

    @Test
    public void testCloseButtonVisibility_DefaultHiddenAndShown() {
        AdaptiveUiSheetCoordinator defaultContent =
                createContent(
                        newModelBuilder()
                                .with(
                                        AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY,
                                        CloseButtonVisibility.DEFAULT)
                                .build());
        assertEquals(
                View.GONE,
                find(defaultContent, R.id.adaptive_ui_sheet_close_button).getVisibility());
        assertEquals(
                View.GONE, find(defaultContent, R.id.adaptive_ui_sheet_header).getVisibility());

        AdaptiveUiSheetCoordinator hiddenContent =
                createContent(
                        newModelBuilder()
                                .with(
                                        AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY,
                                        CloseButtonVisibility.HIDDEN)
                                .build());
        assertEquals(
                View.GONE,
                find(hiddenContent, R.id.adaptive_ui_sheet_close_button).getVisibility());
        assertEquals(View.GONE, find(hiddenContent, R.id.adaptive_ui_sheet_header).getVisibility());

        AdaptiveUiSheetCoordinator shownContent =
                createContent(
                        newModelBuilder()
                                .with(
                                        AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY,
                                        CloseButtonVisibility.SHOWN)
                                .build());
        assertEquals(
                View.VISIBLE,
                find(shownContent, R.id.adaptive_ui_sheet_close_button).getVisibility());
        assertEquals(
                View.VISIBLE, find(shownContent, R.id.adaptive_ui_sheet_header).getVisibility());
    }

    @Test
    public void testCloseButtonClick_DoesNotRunSecondaryCallback() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.TITLE, "Title")
                        .with(
                                AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY,
                                CloseButtonVisibility.SHOWN)
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "No thanks")
                        .with(
                                AdaptiveUiProperties.SECONDARY_BUTTON_BEHAVIOR,
                                SecondaryButtonBehavior.CLOSE)
                        .with(AdaptiveUiProperties.DELEGATE, mDelegate)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        View closeButton = find(content, R.id.adaptive_ui_sheet_close_button);
        View secondaryButton = find(content, R.id.adaptive_ui_sheet_secondary_button);

        assertEquals(View.VISIBLE, closeButton.getVisibility());
        assertEquals(View.VISIBLE, secondaryButton.getVisibility());

        closeButton.performClick();

        verify(mDelegate, never()).onSecondaryButtonClicked();
        assertEquals(List.of(DismissReason.CLOSE_BUTTON), mSheetDismissals);
    }

    @Test
    public void testSecondaryClick_CloseBehaviorDismissesWithoutCallback() {
        String secondaryText = "No thanks";
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.TITLE, "Title")
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, secondaryText)
                        .with(
                                AdaptiveUiProperties.SECONDARY_BUTTON_BEHAVIOR,
                                SecondaryButtonBehavior.CLOSE)
                        .with(AdaptiveUiProperties.DELEGATE, mDelegate)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        TextView secondaryButton = find(content, R.id.adaptive_ui_sheet_secondary_button);
        View closeButton = find(content, R.id.adaptive_ui_sheet_close_button);

        assertEquals(View.VISIBLE, secondaryButton.getVisibility());
        assertEquals(secondaryText, secondaryButton.getText().toString());
        assertEquals(View.GONE, closeButton.getVisibility());

        secondaryButton.performClick();

        verify(mDelegate, never()).onSecondaryButtonClicked();
        assertEquals(List.of(DismissReason.SECONDARY_ACTION), mSheetDismissals);
    }

    @Test
    public void testSecondaryClick_CloseBehaviorRespectsButtonTapProtection() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "No thanks")
                        .with(
                                AdaptiveUiProperties.SECONDARY_BUTTON_BEHAVIOR,
                                SecondaryButtonBehavior.CLOSE)
                        .with(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, 500L)
                        .with(AdaptiveUiProperties.DELEGATE, mDelegate)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        BottomSheetObserver observer = showAndCaptureObserver(content);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);
        observer.onSheetContentChanged(content);
        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        View secondaryButton = find(content, R.id.adaptive_ui_sheet_secondary_button);

        secondaryButton.performClick();
        verify(mDelegate, never()).onSecondaryButtonClicked();
        verify(mBottomSheetController, never()).hideContent(eq(content), anyBoolean(), anyInt());
        assertTrue(mSheetDismissals.isEmpty());

        ShadowSystemClock.advanceBy(Duration.ofMillis(501));
        secondaryButton.performClick();
        verify(mDelegate, never()).onSecondaryButtonClicked();
        verify(mBottomSheetController)
                .hideContent(content, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
        content.destroy();
        assertEquals(List.of(DismissReason.SECONDARY_ACTION), mSheetDismissals);
    }

    @Test
    public void testClicksAfterDestroyAreIgnored() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.DELEGATE, mDelegate)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        View primary = find(content, R.id.adaptive_ui_sheet_primary_button);

        content.destroy();
        primary.performClick();
        content.onBackPressed();

        verify(mDelegate, never()).onPrimaryButtonClicked();
        // Only destroy() reports, with the fallback reason; the later clicks are ignored.
        assertEquals(List.of(DismissReason.NAVIGATION), mSheetDismissals);
    }

    @Test
    public void testBackPress_IgnoredAfterDismissRequested() {
        int[] handled = new int[1];
        OnBackPressedCallback handler =
                new OnBackPressedCallback(/* enabled= */ true) {
                    @Override
                    public void handleOnBackPressed() {
                        handled[0]++;
                    }
                };
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.BACK_PRESS_HANDLER, handler)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        find(content, R.id.adaptive_ui_sheet_primary_button).performClick();
        content.onBackPressed();

        assertEquals(0, handled[0]);
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mSheetDismissals);
    }

    @Test
    public void testBackPress_UnsetHandlerRequestsDismiss() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());

        content.onBackPressed();

        assertEquals(List.of(DismissReason.BACK_PRESS), mSheetDismissals);
    }

    @Test
    public void testBackPress_EnabledHandlerConsumesBackPress() {
        int[] handled = new int[1];
        OnBackPressedCallback handler =
                new OnBackPressedCallback(/* enabled= */ true) {
                    @Override
                    public void handleOnBackPressed() {
                        handled[0]++;
                    }
                };
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.BACK_PRESS_HANDLER, handler).build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        content.onBackPressed();

        assertEquals(1, handled[0]);
        assertTrue(mSheetDismissals.isEmpty());
    }

    @Test
    public void testBackPress_DisabledHandlerFallsBackToDismiss() {
        int[] handled = new int[1];
        OnBackPressedCallback handler =
                new OnBackPressedCallback(/* enabled= */ false) {
                    @Override
                    public void handleOnBackPressed() {
                        handled[0]++;
                    }
                };
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.BACK_PRESS_HANDLER, handler).build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        content.onBackPressed();

        assertEquals(0, handled[0]);
        assertEquals(List.of(DismissReason.BACK_PRESS), mSheetDismissals);
    }

    @Test
    public void testHandleBackPress_ReturnsTrueAndDismisses() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());

        assertTrue(content.handleBackPress());

        assertEquals(List.of(DismissReason.BACK_PRESS), mSheetDismissals);
    }

    // ---------------------------------------------------------------------------------------
    // Latched and custom dismiss reasons.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testOnSheetHiddenThenDestroy_LatchedPrimaryAction() {
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "P").build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);

        find(content, R.id.adaptive_ui_sheet_primary_button).performClick();
        assertTrue(mSheetDismissals.isEmpty());

        content.onSheetHidden(StateChangeReason.INTERACTION_COMPLETE);
        content.destroy();
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mSheetDismissals);
    }

    @Test
    public void testOnSheetHiddenThenDestroy_LatchedSecondaryAction() {
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "S").build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);

        find(content, R.id.adaptive_ui_sheet_secondary_button).performClick();
        assertTrue(mSheetDismissals.isEmpty());

        content.onSheetHidden(StateChangeReason.INTERACTION_COMPLETE);
        content.destroy();
        assertEquals(List.of(DismissReason.SECONDARY_ACTION), mSheetDismissals);
    }

    @Test
    public void testOnSheetHiddenThenDestroy_LatchedCloseButton() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);

        find(content, R.id.adaptive_ui_sheet_close_button).performClick();
        assertTrue(mSheetDismissals.isEmpty());

        content.onSheetHidden(StateChangeReason.CLOSE_BUTTON);
        content.destroy();
        assertEquals(List.of(DismissReason.CLOSE_BUTTON), mSheetDismissals);
    }

    @Test
    public void testOnSheetHiddenThenDestroy_LatchedBackPress() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);

        content.onBackPressed();
        assertTrue(mSheetDismissals.isEmpty());

        content.onSheetHidden(StateChangeReason.BACK_PRESS);
        content.destroy();
        assertEquals(List.of(DismissReason.BACK_PRESS), mSheetDismissals);
    }

    @Test
    public void testOnSheetHiddenThenDestroy_UnlatchedExternalSwipeDown() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());

        content.onSheetHidden(StateChangeReason.SWIPE);
        content.destroy();
        assertEquals(List.of(DismissReason.SWIPE_DOWN), mSheetDismissals);
    }

    @Test
    public void testOnSheetHiddenThenDestroy_UnlatchedExternalTapScrim() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());

        content.onSheetHidden(StateChangeReason.TAP_SCRIM);
        content.destroy();
        assertEquals(List.of(DismissReason.TOUCH_OUTSIDE), mSheetDismissals);
    }

    @Test
    public void testOnSheetHiddenThenDestroy_UnlatchedNavigation() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());

        content.onSheetHidden(StateChangeReason.NAVIGATION);
        content.destroy();
        assertEquals(List.of(DismissReason.NAVIGATION), mSheetDismissals);
    }

    @Test
    public void testOnSheetHiddenThenDestroy_UnlatchedCloseButton() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());

        content.onSheetHidden(StateChangeReason.CLOSE_BUTTON);
        content.destroy();
        assertEquals(List.of(DismissReason.CLOSE_BUTTON), mSheetDismissals);
    }

    @Test
    public void testOnSheetHidden_SuppressionDoesNotDismissUntilDestroy() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());

        // Temporary suppression hides the sheet without destroying it; later swipe-down overwrites
        // the hidden reason before destroy().
        content.onSheetHidden(StateChangeReason.OMNIBOX_FOCUS);
        assertTrue(mSheetDismissals.isEmpty());

        content.onSheetHidden(StateChangeReason.SWIPE);
        content.destroy();
        assertEquals(List.of(DismissReason.SWIPE_DOWN), mSheetDismissals);
    }

    @Test
    public void testDestroy_WithoutOnSheetHidden_DefaultsToNavigation() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());

        // Queued sheet evicted via clearRequestsAndHide() without any HIDDEN state transition.
        content.destroy();
        assertEquals(List.of(DismissReason.NAVIGATION), mSheetDismissals);
    }

    @Test
    public void testSetDismissReason_OverridesContainerCause() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());

        content.setDismissReason(DismissReason.TAB_SWITCHED);
        content.onSheetHidden(StateChangeReason.NONE);
        content.destroy();

        assertEquals(List.of(DismissReason.TAB_SWITCHED), mSheetDismissals);
    }

    @Test
    public void testOnSheetHiddenThenDestroy_Idempotency() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());

        content.onSheetHidden(StateChangeReason.SWIPE);
        content.destroy();
        content.onSheetHidden(StateChangeReason.SWIPE);
        content.destroy();

        assertEquals(List.of(DismissReason.SWIPE_DOWN), mSheetDismissals);
    }

    @Test
    public void testDestroyWithoutHidden_ReportsFallbackReasonOnce() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());

        content.destroy();
        content.onSheetHidden(StateChangeReason.SWIPE);
        content.destroy();

        assertEquals(List.of(DismissReason.NAVIGATION), mSheetDismissals);
    }

    private @DismissReason int closeSheetWithReason(@StateChangeReason int reason) {
        mSheetDismissals.clear();
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());
        content.onSheetHidden(reason);
        content.destroy();
        assertEquals(1, mSheetDismissals.size());
        return mSheetDismissals.get(0);
    }

    @Test
    public void testToStateChangeReason() {
        assertEquals(
                StateChangeReason.BACK_PRESS,
                AdaptiveUiSheetCoordinator.toStateChangeReason(DismissReason.BACK_PRESS));
        assertEquals(
                StateChangeReason.TAP_SCRIM,
                AdaptiveUiSheetCoordinator.toStateChangeReason(DismissReason.TOUCH_OUTSIDE));
        assertEquals(
                StateChangeReason.SWIPE,
                AdaptiveUiSheetCoordinator.toStateChangeReason(DismissReason.SWIPE_DOWN));
        assertEquals(
                StateChangeReason.CLOSE_BUTTON,
                AdaptiveUiSheetCoordinator.toStateChangeReason(DismissReason.CLOSE_BUTTON));
        assertEquals(
                StateChangeReason.INTERACTION_COMPLETE,
                AdaptiveUiSheetCoordinator.toStateChangeReason(DismissReason.PRIMARY_ACTION));
        assertEquals(
                StateChangeReason.INTERACTION_COMPLETE,
                AdaptiveUiSheetCoordinator.toStateChangeReason(DismissReason.SECONDARY_ACTION));
        assertEquals(
                StateChangeReason.NONE,
                AdaptiveUiSheetCoordinator.toStateChangeReason(DismissReason.NAVIGATION));
        assertEquals(
                StateChangeReason.NONE,
                AdaptiveUiSheetCoordinator.toStateChangeReason(DismissReason.TAB_SWITCHED));
        assertEquals(
                StateChangeReason.NONE,
                AdaptiveUiSheetCoordinator.toStateChangeReason(DismissReason.TAB_CLOSED));
        assertEquals(
                StateChangeReason.NONE,
                AdaptiveUiSheetCoordinator.toStateChangeReason(DismissReason.DISMISSED_BY_FEATURE));
    }

    @Test
    public void testOnSheetHiddenThenDestroy_TranslatesUnlatchedStateChangeReasons() {
        assertEquals(DismissReason.BACK_PRESS, closeSheetWithReason(StateChangeReason.BACK_PRESS));
        assertEquals(
                DismissReason.TOUCH_OUTSIDE, closeSheetWithReason(StateChangeReason.TAP_SCRIM));
        assertEquals(DismissReason.SWIPE_DOWN, closeSheetWithReason(StateChangeReason.SWIPE));
        assertEquals(
                DismissReason.CLOSE_BUTTON, closeSheetWithReason(StateChangeReason.CLOSE_BUTTON));
        assertEquals(
                DismissReason.PRIMARY_ACTION,
                closeSheetWithReason(StateChangeReason.INTERACTION_COMPLETE));
        assertEquals(DismissReason.NAVIGATION, closeSheetWithReason(StateChangeReason.NAVIGATION));
        assertEquals(DismissReason.NAVIGATION, closeSheetWithReason(StateChangeReason.NONE));
        assertEquals(
                DismissReason.NAVIGATION, closeSheetWithReason(StateChangeReason.COMPOSITED_UI));
        assertEquals(DismissReason.NAVIGATION, closeSheetWithReason(StateChangeReason.VR));
        assertEquals(DismissReason.NAVIGATION, closeSheetWithReason(StateChangeReason.PROMOTE_TAB));
        assertEquals(
                DismissReason.NAVIGATION, closeSheetWithReason(StateChangeReason.OMNIBOX_FOCUS));
    }

    // ---------------------------------------------------------------------------------------
    // BottomSheetContent contract.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testHasCustomLifecycleReflectsScope() {
        PropertyModel windowModel = newModelBuilder().build();
        assertTrue(createContent(windowModel, AdaptiveUiScope.WINDOW).hasCustomLifecycle());

        PropertyModel transientModel = newModelBuilder().build();
        assertFalse(createContent(transientModel, AdaptiveUiScope.TRANSIENT).hasCustomLifecycle());
    }

    @Test
    public void testVerticalScrollOffsetReflectsScrollView() {
        View customView = new View(mActivity);
        customView.setLayoutParams(new FrameLayout.LayoutParams(100, 3000));
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.CUSTOM_VIEW, customView).build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        View root = content.getContentView();
        root.measure(
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(600, View.MeasureSpec.EXACTLY));
        root.layout(0, 0, 1000, 600);
        ScrollView scrollView = find(content, R.id.adaptive_ui_sheet_scroll_view);

        assertEquals(0, content.getVerticalScrollOffset());

        scrollView.setScrollY(100);

        assertEquals(scrollView.getScrollY(), content.getVerticalScrollOffset());
        assertEquals(100, content.getVerticalScrollOffset());
    }

    @Test
    public void testContentViewAndToolbar() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());

        assertNotNull(content.getContentView());
        assertNull(content.getToolbarView());
        assertTrue(content.swipeToDismissEnabled());
    }

    @Test
    public void testShowHandlebarReflectsModel() {
        PropertyModel defaultModel = newModelBuilder().build();
        assertFalse(createContent(defaultModel).showHandlebar());

        PropertyModel handlebarModel =
                newModelBuilder().with(AdaptiveUiProperties.SHOW_SHEET_HANDLEBAR, true).build();
        assertTrue(createContent(handlebarModel).showHandlebar());
    }

    @Test
    public void testGestureDismissalDisabled_ControlsSwipeToDismissEnabledAndUpdatesLive() {
        PropertyModel model = newModelBuilder().build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        assertTrue(content.swipeToDismissEnabled());

        model.set(AdaptiveUiProperties.GESTURE_DISMISSAL_DISABLED, true);
        assertFalse(content.swipeToDismissEnabled());

        model.set(AdaptiveUiProperties.GESTURE_DISMISSAL_DISABLED, false);
        assertTrue(content.swipeToDismissEnabled());

        PropertyModel disabledModel =
                newModelBuilder()
                        .with(AdaptiveUiProperties.GESTURE_DISMISSAL_DISABLED, true)
                        .build();
        AdaptiveUiSheetCoordinator disabledContent = createContent(disabledModel);
        assertFalse(disabledContent.swipeToDismissEnabled());

        disabledContent.onBackPressed();
        assertEquals(List.of(DismissReason.BACK_PRESS), mSheetDismissals);
    }

    @Test
    public void testAccessibilityStringIds_UnsetDefaults() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());

        assertEquals(
                R.string.bottom_sheet_accessibility_description,
                content.getSheetHalfHeightAccessibilityStringId());
        assertEquals(
                R.string.bottom_sheet_accessibility_description,
                content.getSheetFullHeightAccessibilityStringId());
        assertEquals(
                R.string.bottom_sheet_accessibility_description,
                content.getSheetClosedAccessibilityStringId());
        assertEquals(
                R.string.bottom_sheet_accessibility_description,
                content.getSheetHiddenAccessibilityStringId());
    }

    @Test
    public void testAccessibilityStringIds_OpenedOnly() {
        PropertyModel model =
                newModelBuilder()
                        .with(
                                AdaptiveUiProperties.SHEET_OPENED_ACCESSIBILITY_STRING_ID,
                                android.R.string.ok)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        assertEquals(android.R.string.ok, content.getSheetHalfHeightAccessibilityStringId());
        assertEquals(android.R.string.ok, content.getSheetFullHeightAccessibilityStringId());
        assertEquals(
                R.string.bottom_sheet_accessibility_description,
                content.getSheetClosedAccessibilityStringId());
        assertEquals(
                R.string.bottom_sheet_accessibility_description,
                content.getSheetHiddenAccessibilityStringId());
    }

    @Test
    public void testAccessibilityStringIds_ClosedOnly() {
        PropertyModel model =
                newModelBuilder()
                        .with(
                                AdaptiveUiProperties.SHEET_CLOSED_ACCESSIBILITY_STRING_ID,
                                android.R.string.cancel)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        assertEquals(
                R.string.bottom_sheet_accessibility_description,
                content.getSheetHalfHeightAccessibilityStringId());
        assertEquals(
                R.string.bottom_sheet_accessibility_description,
                content.getSheetFullHeightAccessibilityStringId());
        assertEquals(android.R.string.cancel, content.getSheetClosedAccessibilityStringId());
        assertEquals(android.R.string.cancel, content.getSheetHiddenAccessibilityStringId());
    }

    @Test
    public void testAccessibilityStringIds_BothSet() {
        PropertyModel model =
                newModelBuilder()
                        .with(
                                AdaptiveUiProperties.SHEET_OPENED_ACCESSIBILITY_STRING_ID,
                                android.R.string.ok)
                        .with(
                                AdaptiveUiProperties.SHEET_CLOSED_ACCESSIBILITY_STRING_ID,
                                android.R.string.cancel)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        assertEquals(android.R.string.ok, content.getSheetHalfHeightAccessibilityStringId());
        assertEquals(android.R.string.ok, content.getSheetFullHeightAccessibilityStringId());
        assertEquals(android.R.string.cancel, content.getSheetClosedAccessibilityStringId());
        assertEquals(android.R.string.cancel, content.getSheetHiddenAccessibilityStringId());
    }

    // ---------------------------------------------------------------------------------------
    // Driving the BottomSheetController.
    // ---------------------------------------------------------------------------------------

    private BottomSheetObserver showAndCaptureObserver(AdaptiveUiSheetCoordinator content) {
        content.show();
        ArgumentCaptor<BottomSheetObserver> captor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mBottomSheetController).addObserver(captor.capture());
        verify(mBottomSheetController).requestShowContent(content, /* animate= */ true);
        return captor.getValue();
    }

    @Test
    public void testOnShown_OnlyOnceAndOnlyForOwnContent() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());
        BottomSheetObserver observer = showAndCaptureObserver(content);

        observer.onSheetContentChanged(mOtherSheetContent);
        assertEquals(0, mShownCount);

        observer.onSheetContentChanged(content);
        // Put back in the queue and shown again.
        observer.onSheetContentChanged(mOtherSheetContent);
        observer.onSheetContentChanged(content);
        assertEquals(1, mShownCount);
    }

    @Test
    public void testIsCurrent_ReflectsCurrentSheetContent() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());
        showAndCaptureObserver(content);
        assertFalse(content.isCurrent());

        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);
        assertTrue(content.isCurrent());

        // Another sheet takes its place and this one goes back in the queue.
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mOtherSheetContent);
        assertFalse(content.isCurrent());
    }

    @Test
    public void testHidden_SuppressedSheetIsNotDismissed() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());
        BottomSheetObserver observer = showAndCaptureObserver(content);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);

        observer.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.OMNIBOX_FOCUS);
        assertTrue(mSheetDismissals.isEmpty());

        // Restored later, then swiped away; the latest HIDDEN reason wins.
        observer.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.SWIPE);
        content.destroy();
        assertEquals(List.of(DismissReason.SWIPE_DOWN), mSheetDismissals);
        verify(mBottomSheetController).removeObserver(observer);
    }

    @Test
    public void testClosed_ReasonKeptWhenDestroyedBeforeHidden() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());
        BottomSheetObserver observer = showAndCaptureObserver(content);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);

        // With BottomSheetDeferContentSwapOnHidden off, the BottomSheetController destroys the
        // content before this observer gets the HIDDEN state change.
        observer.onSheetClosed(StateChangeReason.SWIPE);
        content.destroy();

        assertEquals(List.of(DismissReason.SWIPE_DOWN), mSheetDismissals);
    }

    @Test
    public void testHidden_OtherContentIgnored() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());
        BottomSheetObserver observer = showAndCaptureObserver(content);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mOtherSheetContent);

        observer.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.SWIPE);
        content.destroy();

        assertEquals(List.of(DismissReason.NAVIGATION), mSheetDismissals);
    }

    @Test
    public void testHide_CurrentSheetWaitsForDestroy() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());
        showAndCaptureObserver(content);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);

        content.hide(DismissReason.SECONDARY_ACTION, /* animate= */ true);
        verify(mBottomSheetController)
                .hideContent(content, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
        assertTrue(mSheetDismissals.isEmpty());

        // The BottomSheetController destroys the hidden content; the latched reason wins.
        content.destroy();
        assertEquals(List.of(DismissReason.SECONDARY_ACTION), mSheetDismissals);
    }

    @Test
    public void testHide_QueuedSheetIsDismissedRightAway() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());
        showAndCaptureObserver(content);

        content.hide(DismissReason.NAVIGATION, /* animate= */ true);

        verify(mBottomSheetController).hideContent(eq(content), anyBoolean(), anyInt());
        assertEquals(List.of(DismissReason.NAVIGATION), mSheetDismissals);
        assertEquals(0, mShownCount);
    }

    @Test
    public void testHide_WithoutAnimationIsDismissedRightAway() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());
        showAndCaptureObserver(content);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);

        content.hide(DismissReason.TAB_CLOSED, /* animate= */ false);

        assertEquals(List.of(DismissReason.TAB_CLOSED), mSheetDismissals);
    }

    @Test
    public void testRequeuedAfterInSheetClose_AcceptsInputAgain() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.DELEGATE, mDelegate)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        BottomSheetObserver observer = showAndCaptureObserver(content);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);
        observer.onSheetContentChanged(content);
        View primaryButton = find(content, R.id.adaptive_ui_sheet_primary_button);

        // The primary button requests a close; while it is in flight, input is ignored.
        primaryButton.performClick();
        primaryButton.performClick();
        verify(mDelegate).onPrimaryButtonClicked();
        assertTrue(mSheetDismissals.isEmpty());

        // Higher-priority content pushed the sheet back and it is shown again: the close never
        // happened, so the sheet must accept input again.
        observer.onSheetContentChanged(mOtherSheetContent);
        observer.onSheetContentChanged(content);
        primaryButton.performClick();
        verify(mDelegate, times(2)).onPrimaryButtonClicked();
        assertEquals(1, mShownCount);

        content.destroy();
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mSheetDismissals);
    }

    @Test
    public void testShow_ReturnsRequestShowContentResult() {
        AdaptiveUiSheetCoordinator shownContent = createContent(newModelBuilder().build());
        when(mBottomSheetController.requestShowContent(shownContent, /* animate= */ true))
                .thenReturn(true);
        assertTrue(shownContent.show());

        AdaptiveUiSheetCoordinator queuedContent = createContent(newModelBuilder().build());
        when(mBottomSheetController.requestShowContent(queuedContent, /* animate= */ true))
                .thenReturn(false);
        assertFalse(queuedContent.show());

        queuedContent.destroy();
        assertFalse(queuedContent.show());
    }

    @Test
    public void testHide_AfterDismissalIsIgnored() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());
        showAndCaptureObserver(content);
        content.destroy();

        content.hide(DismissReason.CLOSE_BUTTON, /* animate= */ true);

        verify(mBottomSheetController, never()).hideContent(eq(content), anyBoolean(), anyInt());
        assertEquals(1, mSheetDismissals.size());
    }

    // ---------------------------------------------------------------------------------------
    // Self-scrolling custom view.
    // ---------------------------------------------------------------------------------------

    /** Returns a scroll view whose content is taller than any sheet used in these tests. */
    private ScrollView newScrollingBody() {
        ScrollView body = new ScrollView(mActivity);
        View tallContent = new View(mActivity);
        tallContent.setMinimumHeight(TALL_CONTENT_HEIGHT_PX);
        body.addView(tallContent);
        return body;
    }

    /** Lays the sheet content out the way a sheet sized to its content does. */
    private static void layoutAsSheet(View root) {
        root.measure(
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(600, View.MeasureSpec.AT_MOST));
        root.layout(0, 0, 1000, root.getMeasuredHeight());
    }

    @Test
    public void testSelfScrollingCustomView_PinnedBetweenDescriptionAndFooter() {
        ScrollView body = newScrollingBody();
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.DESCRIPTION, "Description")
                        .with(AdaptiveUiProperties.FOOTER_MESSAGE, "Footer")
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, body)
                        .with(AdaptiveUiProperties.CUSTOM_VIEW_SCROLLS_ITSELF, true)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        View root = content.getContentView();
        layoutAsSheet(root);

        // The body is not inside a framework scroll view, and is cut down to the space left.
        assertNull(root.findViewById(R.id.adaptive_ui_sheet_scroll_view));
        ViewGroup container = find(content, R.id.adaptive_ui_sheet_custom_view_container);
        assertSame(container, body.getParent());
        assertEquals(600, root.getHeight());
        assertEquals(1000, container.getWidth());
        assertTrue(body.getHeight() > 0);
        assertTrue(body.getHeight() < TALL_CONTENT_HEIGHT_PX);

        View description = find(content, R.id.adaptive_ui_sheet_description);
        View footer = find(content, R.id.adaptive_ui_sheet_footer_message);
        assertEquals(View.VISIBLE, description.getVisibility());
        assertEquals(View.VISIBLE, footer.getVisibility());
        assertTrue(description.getBottom() <= container.getTop());
        assertTrue(container.getBottom() <= footer.getTop());
        assertTrue(find(content, R.id.adaptive_ui_sheet_button_bar).getBottom() <= 600);
    }

    @Test
    public void testVerticalScrollOffset_SelfScrollingCustomView() {
        ScrollView body = newScrollingBody();
        // The custom view's root doesn't scroll; a view inside it does.
        FrameLayout customView = new FrameLayout(mActivity);
        customView.addView(body);
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, customView)
                        .with(AdaptiveUiProperties.CUSTOM_VIEW_SCROLLS_ITSELF, true)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        layoutAsSheet(content.getContentView());

        assertEquals(0, content.getVerticalScrollOffset());

        body.scrollTo(0, 100);
        assertEquals(100, body.getScrollY());
        assertEquals(1, content.getVerticalScrollOffset());

        // A scrolled view that is not showing (such as another screen of the body) is ignored.
        body.setVisibility(View.INVISIBLE);
        assertEquals(0, content.getVerticalScrollOffset());

        body.setVisibility(View.VISIBLE);
        body.scrollTo(0, 0);
        assertEquals(0, content.getVerticalScrollOffset());
    }

    @Test
    public void testVerticalScrollOffset_SelfScrollingWithoutCustomView() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.CUSTOM_VIEW_SCROLLS_ITSELF, true)
                        .build();

        assertEquals(0, createContent(model).getVerticalScrollOffset());
    }

    // ---------------------------------------------------------------------------------------
    // Body scroll view focusability.
    // ---------------------------------------------------------------------------------------

    @Test
    public void testBodyScrollView_NotFocusableWhenCustomViewFits() {
        View shortCustomView = new View(mActivity);
        shortCustomView.setLayoutParams(
                new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 100));
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, shortCustomView)
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        layoutAsSheet(content.getContentView());

        ScrollView scrollView = find(content, R.id.adaptive_ui_sheet_scroll_view);
        assertFalse(scrollView.canScrollVertically(-1));
        assertFalse(scrollView.canScrollVertically(1));
        assertFalse(scrollView.isFocusable());
    }

    @Test
    public void testBodyScrollView_TabOrderSkipsScrollViewWhenCustomViewFits() {
        ShadowInstrumentation.getInstrumentation().setInTouchMode(false);
        View shortCustomView = new View(mActivity);
        shortCustomView.setLayoutParams(
                new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 100));
        PropertyModel model =
                newModelBuilder()
                        .with(
                                AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY,
                                CloseButtonVisibility.SHOWN)
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, shortCustomView)
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        mActivity.setContentView(content.getContentView());
        mActivity.getWindow().getDecorView().layout(0, 0, 1000, 1000);
        layoutAsSheet(content.getContentView());

        View closeButton = find(content, R.id.adaptive_ui_sheet_close_button);
        View primaryButton = find(content, R.id.adaptive_ui_sheet_primary_button);
        assertSame(primaryButton, closeButton.focusSearch(View.FOCUS_FORWARD));
    }

    @Test
    public void testBodyScrollView_FocusableWhenCustomViewScrolls() {
        View tallCustomView = new View(mActivity);
        tallCustomView.setLayoutParams(
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, TALL_CONTENT_HEIGHT_PX));
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, tallCustomView)
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        layoutAsSheet(content.getContentView());

        ScrollView scrollView = find(content, R.id.adaptive_ui_sheet_scroll_view);
        assertTrue(scrollView.canScrollVertically(1));
        assertTrue(scrollView.isFocusable());
    }
}
