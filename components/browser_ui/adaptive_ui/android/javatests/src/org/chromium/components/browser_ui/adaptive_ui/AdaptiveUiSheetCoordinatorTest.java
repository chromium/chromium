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
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

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
                        AdaptiveUiProperties.BACK_PRESS_HANDLER,
                        AdaptiveUiProperties.ON_PRIMARY_BUTTON_CLICKED,
                        AdaptiveUiProperties.ON_SECONDARY_BUTTON_CLICKED,
                        AdaptiveUiProperties.PRIMARY_BUTTON_KEEPS_OPEN,
                        AdaptiveUiProperties.SECONDARY_BUTTON_KEEPS_OPEN);
        assertEquals(expected, AdaptiveUiSheetViewBinder.NON_VISUAL_SHEET_KEYS);
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

        AdaptiveUiSheetCoordinator transientContent =
                createContent(newModelBuilder().build(), AdaptiveUiScope.TRANSIENT);
        assertTrue(transientContent.getSheetType().isModal());
        assertFalse(transientContent.getSheetType().isPersistent());
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
    public void testHide_AfterDismissalIsIgnored() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());
        showAndCaptureObserver(content);
        content.destroy();

        content.hide(DismissReason.CLOSE_BUTTON, /* animate= */ true);

        verify(mBottomSheetController, never()).hideContent(eq(content), anyBoolean(), anyInt());
        assertEquals(1, mSheetDismissals.size());
    }
}
