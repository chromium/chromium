// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
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
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
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
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private BottomSheetContent mOtherSheetContent;

    private Activity mActivity;
    private int mShownCount;
    private final List<Integer> mSheetDismissals = new ArrayList<>();

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
    }

    private AdaptiveUiSheetCoordinator createContent(PropertyModel model) {
        return createContent(model, AdaptiveUiScope.TRANSIENT);
    }

    private AdaptiveUiSheetCoordinator createContent(
            PropertyModel model, @AdaptiveUiScope int scope) {
        return createContent(model, scope, /* isUserInitiated= */ false, UserCriticalFeature.NONE);
    }

    private AdaptiveUiSheetCoordinator createContent(
            PropertyModel model,
            @AdaptiveUiScope int scope,
            boolean isUserInitiated,
            @UserCriticalFeature int userCriticalFeature) {
        return new AdaptiveUiSheetCoordinator(
                mActivity,
                model,
                scope,
                isUserInitiated,
                userCriticalFeature,
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
                        AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION,
                        AdaptiveUiProperties.SHEET_OPENED_ACCESSIBILITY_STRING_ID,
                        AdaptiveUiProperties.SHEET_CLOSED_ACCESSIBILITY_STRING_ID,
                        AdaptiveUiProperties.BACK_PRESS_HANDLER,
                        AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED,
                        AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED,
                        AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN,
                        AdaptiveUiProperties.SECONDARY_BUTTON_KEEPS_OPEN,
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
        int[] primaryClicks = new int[1];
        PropertyModel primaryModel =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(
                                AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED,
                                () -> primaryClicks[0]++)
                        .build();
        assertEquals(0L, primaryModel.get(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS));
        AdaptiveUiSheetCoordinator primaryContent = createContent(primaryModel);
        BottomSheetObserver primaryObserver = showAndCaptureObserver(primaryContent);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(primaryContent);
        primaryObserver.onSheetContentChanged(primaryContent);

        find(primaryContent, R.id.adaptive_ui_sheet_primary_button).performClick();
        assertEquals(1, primaryClicks[0]);
        verify(mBottomSheetController)
                .hideContent(
                        primaryContent,
                        /* animate= */ true,
                        StateChangeReason.INTERACTION_COMPLETE);
        primaryContent.destroy();

        clearInvocations(mBottomSheetController);

        int[] secondaryClicks = new int[1];
        PropertyModel secondaryModel =
                newModelBuilder()
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .with(
                                AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED,
                                () -> secondaryClicks[0]++)
                        .build();
        AdaptiveUiSheetCoordinator secondaryContent = createContent(secondaryModel);
        BottomSheetObserver secondaryObserver = showAndCaptureObserver(secondaryContent);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(secondaryContent);
        secondaryObserver.onSheetContentChanged(secondaryContent);

        find(secondaryContent, R.id.adaptive_ui_sheet_secondary_button).performClick();
        assertEquals(1, secondaryClicks[0]);
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
        int[] primaryClicks = new int[1];
        int[] secondaryClicks = new int[1];
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(
                                AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED,
                                () -> primaryClicks[0]++)
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .with(
                                AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED,
                                () -> secondaryClicks[0]++)
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_KEEPS_OPEN, true)
                        .with(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, 500L)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        BottomSheetObserver observer = showAndCaptureObserver(content);
        View primary = find(content, R.id.adaptive_ui_sheet_primary_button);
        View secondary = find(content, R.id.adaptive_ui_sheet_secondary_button);

        // Clicks before onSheetContentChanged(content) are blocked even if time advances.
        ShadowSystemClock.advanceBy(Duration.ofSeconds(1));
        primary.performClick();
        secondary.performClick();
        assertEquals(0, primaryClicks[0]);
        assertEquals(0, secondaryClicks[0]);
        verify(mBottomSheetController, never()).hideContent(eq(content), anyBoolean(), anyInt());

        // Sheet becomes current and settles open at t0.
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);
        observer.onSheetContentChanged(content);
        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);

        // Clicks at +0ms and +300ms are ignored and each resets the protection start timestamp.
        primary.performClick();
        assertEquals(0, primaryClicks[0]);

        ShadowSystemClock.advanceBy(Duration.ofMillis(300));
        primary.performClick();
        assertEquals(0, primaryClicks[0]);

        // At +600ms from show (300ms after the premature tap, <= 500ms), click is still blocked.
        ShadowSystemClock.advanceBy(Duration.ofMillis(300));
        primary.performClick();
        assertEquals(0, primaryClicks[0]);

        // Exact inclusive boundary (+500ms since last tap) is also blocked and resets the timer.
        ShadowSystemClock.advanceBy(Duration.ofMillis(500));
        secondary.performClick();
        assertEquals(0, secondaryClicks[0]);

        // Advancing +501ms allows the secondary click and restarts the protection window.
        ShadowSystemClock.advanceBy(Duration.ofMillis(501));
        secondary.performClick();
        assertEquals(1, secondaryClicks[0]);
        primary.performClick();
        assertEquals(0, primaryClicks[0]);

        // Advancing another +501ms allows the primary click and dismisses the sheet.
        ShadowSystemClock.advanceBy(Duration.ofMillis(501));
        primary.performClick();
        assertEquals(1, primaryClicks[0]);
        verify(mBottomSheetController)
                .hideContent(content, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
        content.destroy();
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mSheetDismissals);
    }

    @Test
    public void testButtonTapProtection_CloseButtonAndBackPressUnaffected() {
        int[] primaryClicks = new int[1];
        PropertyModel closeModel =
                newModelBuilder()
                        .with(AdaptiveUiProperties.SHOW_CLOSE_BUTTON, true)
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(
                                AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED,
                                () -> primaryClicks[0]++)
                        .with(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, 500L)
                        .build();
        AdaptiveUiSheetCoordinator closeContent = createContent(closeModel);
        BottomSheetObserver closeObserver = showAndCaptureObserver(closeContent);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(closeContent);
        closeObserver.onSheetContentChanged(closeContent);
        closeObserver.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);

        find(closeContent, R.id.adaptive_ui_sheet_primary_button).performClick();
        assertEquals(0, primaryClicks[0]);

        find(closeContent, R.id.adaptive_ui_sheet_close_button).performClick();
        verify(mBottomSheetController)
                .hideContent(closeContent, /* animate= */ true, StateChangeReason.CLOSE_BUTTON);
        closeContent.destroy();

        clearInvocations(mBottomSheetController);

        PropertyModel backModel =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(
                                AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED,
                                () -> primaryClicks[0]++)
                        .with(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, 500L)
                        .build();
        AdaptiveUiSheetCoordinator backContent = createContent(backModel);
        BottomSheetObserver backObserver = showAndCaptureObserver(backContent);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(backContent);
        backObserver.onSheetContentChanged(backContent);
        backObserver.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);

        find(backContent, R.id.adaptive_ui_sheet_primary_button).performClick();
        assertEquals(0, primaryClicks[0]);

        backContent.onBackPressed();
        verify(mBottomSheetController)
                .hideContent(backContent, /* animate= */ true, StateChangeReason.BACK_PRESS);
        backContent.destroy();

        assertEquals(
                List.of(DismissReason.CLOSE_BUTTON, DismissReason.BACK_PRESS), mSheetDismissals);
    }

    @Test
    public void testButtonTapProtection_BlocksDuringSlideInUntilSettled() {
        int[] clicks = new int[1];
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED, () -> clicks[0]++)
                        .with(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, 100L)
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
        assertEquals(0, clicks[0]);

        observer.onSheetStateChanged(SheetState.PEEK, StateChangeReason.NONE);
        ShadowSystemClock.advanceBy(Duration.ofMillis(150));
        primary.performClick();
        assertEquals(0, clicks[0]);

        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);

        // Later drags (FULL -> SCROLLING -> FULL) don't restart the period.
        ShadowSystemClock.advanceBy(Duration.ofMillis(50));
        observer.onSheetStateChanged(SheetState.SCROLLING, StateChangeReason.SWIPE);
        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.SWIPE);

        ShadowSystemClock.advanceBy(Duration.ofMillis(51));
        primary.performClick();
        assertEquals(1, clicks[0]);
        verify(mBottomSheetController)
                .hideContent(content, /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
        content.destroy();
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mSheetDismissals);
    }

    @Test
    public void testButtonTapProtection_ReshowResetsPeriod() {
        int[] clicks = new int[1];
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED, () -> clicks[0]++)
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN, true)
                        .with(AdaptiveUiProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, 500L)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        BottomSheetObserver observer = showAndCaptureObserver(content);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);
        observer.onSheetContentChanged(content);
        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        View primary = find(content, R.id.adaptive_ui_sheet_primary_button);

        ShadowSystemClock.advanceBy(Duration.ofMillis(501));
        primary.performClick();
        assertEquals(1, clicks[0]);
        ShadowSystemClock.advanceBy(Duration.ofMillis(501));

        // Swapping into an already-open sheet (FULL) arms immediately without a state change.
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mOtherSheetContent);
        observer.onSheetContentChanged(mOtherSheetContent);
        ShadowSystemClock.advanceBy(Duration.ofSeconds(1));
        primary.performClick();
        assertEquals(1, clicks[0]);

        when(mBottomSheetController.getSheetState()).thenReturn(SheetState.FULL);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);
        observer.onSheetContentChanged(content);
        primary.performClick();
        assertEquals(1, clicks[0]);

        ShadowSystemClock.advanceBy(Duration.ofMillis(501));
        primary.performClick();
        assertEquals(2, clicks[0]);
        ShadowSystemClock.advanceBy(Duration.ofMillis(501));

        // Hiding and re-opening while still current (no content swap) stays blocked through
        // SCROLLING and re-arms on the settled state.
        when(mBottomSheetController.getSheetState()).thenReturn(SheetState.HIDDEN);
        observer.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.OMNIBOX_FOCUS);
        ShadowSystemClock.advanceBy(Duration.ofSeconds(1));
        primary.performClick();
        assertEquals(2, clicks[0]);

        observer.onSheetStateChanged(SheetState.NONE, StateChangeReason.NONE);
        primary.performClick();
        assertEquals(2, clicks[0]);

        observer.onSheetStateChanged(SheetState.SCROLLING, StateChangeReason.NONE);
        ShadowSystemClock.advanceBy(Duration.ofSeconds(1));
        primary.performClick();
        assertEquals(2, clicks[0]);

        observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        primary.performClick();
        assertEquals(2, clicks[0]);

        ShadowSystemClock.advanceBy(Duration.ofMillis(501));
        primary.performClick();
        assertEquals(3, clicks[0]);
    }

    @Test
    public void testFilterTouchForSecurity_DefaultFalse() {
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.SHOW_CLOSE_BUTTON, true)
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
        int[] primaryClicks = new int[1];
        int[] secondaryClicks = new int[1];
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.SHOW_CLOSE_BUTTON, true)
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(
                                AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED,
                                () -> primaryClicks[0]++)
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN, true)
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .with(
                                AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED,
                                () -> secondaryClicks[0]++)
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_KEEPS_OPEN, true)
                        .with(AdaptiveUiProperties.FILTER_TOUCH_FOR_SECURITY, true)
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
        assertEquals(0, primaryClicks[0]);
        assertEquals(0, secondaryClicks[0]);

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
        assertEquals(1, primaryClicks[0]);
        assertEquals(1, secondaryClicks[0]);

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
        assertAllChildrenGone(find(content, R.id.adaptive_ui_sheet_header));
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
                        .with(AdaptiveUiProperties.SHOW_CLOSE_BUTTON, true)
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
        assertTrue(title.getBottom() <= header.getHeight());
        int titleCenter = title.getTop() + title.getBottom();
        int closeCenter = close.getTop() + close.getBottom();
        assertTrue(Math.abs(titleCenter - closeCenter) <= 1);
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
                        newModelBuilder().build(),
                        AdaptiveUiScope.TRANSIENT,
                        /* isUserInitiated= */ true,
                        UserCriticalFeature.ACCOUNT_PICKER);
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
                        .with(AdaptiveUiProperties.SHOW_CLOSE_BUTTON, true)
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

    // ---------------------------------------------------------------------------------------
    // Live updates.
    // ---------------------------------------------------------------------------------------

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
        int[] clicks = new int[1];
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED, () -> clicks[0]++)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        find(content, R.id.adaptive_ui_sheet_primary_button).performClick();

        assertEquals(1, clicks[0]);
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mSheetDismissals);
    }

    @Test
    public void testSecondaryClick_RunsCallbackAndRequestsDismiss() {
        int[] clicks = new int[1];
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .with(AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED, () -> clicks[0]++)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        find(content, R.id.adaptive_ui_sheet_secondary_button).performClick();

        assertEquals(1, clicks[0]);
        assertEquals(List.of(DismissReason.SECONDARY_ACTION), mSheetDismissals);
    }

    @Test
    public void testPrimaryClick_KeepsOpen() {
        int[] clicks = new int[1];
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED, () -> clicks[0]++)
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN, true)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        View primary = find(content, R.id.adaptive_ui_sheet_primary_button);
        primary.performClick();
        primary.performClick();

        assertEquals(2, clicks[0]);
        assertTrue(mSheetDismissals.isEmpty());
    }

    @Test
    public void testSecondaryClick_KeepsOpen() {
        int[] clicks = new int[1];
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT, "Secondary")
                        .with(AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED, () -> clicks[0]++)
                        .with(AdaptiveUiProperties.SECONDARY_BUTTON_KEEPS_OPEN, true)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        find(content, R.id.adaptive_ui_sheet_secondary_button).performClick();

        assertEquals(1, clicks[0]);
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
                newModelBuilder().with(AdaptiveUiProperties.SHOW_CLOSE_BUTTON, true).build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        find(content, R.id.adaptive_ui_sheet_close_button).performClick();

        assertEquals(List.of(DismissReason.CLOSE_BUTTON), mSheetDismissals);
    }

    @Test
    public void testClicksAfterDestroyAreIgnored() {
        int[] clicks = new int[1];
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED, () -> clicks[0]++)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        View primary = find(content, R.id.adaptive_ui_sheet_primary_button);

        content.destroy();
        primary.performClick();
        content.onBackPressed();

        assertEquals(0, clicks[0]);
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
        int[] clicks = new int[1];
        PropertyModel model =
                newModelBuilder()
                        .with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary")
                        .with(AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED, () -> clicks[0]++)
                        .build();
        AdaptiveUiSheetCoordinator content = createContent(model);
        BottomSheetObserver observer = showAndCaptureObserver(content);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(content);
        observer.onSheetContentChanged(content);
        View primaryButton = find(content, R.id.adaptive_ui_sheet_primary_button);

        // The primary button requests a close; while it is in flight, input is ignored.
        primaryButton.performClick();
        primaryButton.performClick();
        assertEquals(1, clicks[0]);
        assertTrue(mSheetDismissals.isEmpty());

        // Higher-priority content pushed the sheet back and it is shown again: the close never
        // happened, so the sheet must accept input again.
        observer.onSheetContentChanged(mOtherSheetContent);
        observer.onSheetContentChanged(content);
        primaryButton.performClick();
        assertEquals(2, clicks[0]);
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
}
