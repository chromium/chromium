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
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link AdaptiveUiSheetCoordinator} and {@link AdaptiveUiSheetViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AdaptiveUiSheetCoordinatorTest {
    private Activity mActivity;
    private final List<Integer> mDismissRequests = new ArrayList<>();

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
    }

    private AdaptiveUiSheetCoordinator createContent(PropertyModel model) {
        return createContent(model, AdaptiveUiScope.TRANSIENT);
    }

    private AdaptiveUiSheetCoordinator createContent(
            PropertyModel model, @AdaptiveUiScope int scope) {
        return new AdaptiveUiSheetCoordinator(mActivity, model, scope, mDismissRequests::add);
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
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mDismissRequests);
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
        assertEquals(List.of(DismissReason.SECONDARY_ACTION), mDismissRequests);
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
        assertTrue(mDismissRequests.isEmpty());
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
        assertTrue(mDismissRequests.isEmpty());
    }

    @Test
    public void testClickWithoutCallback_StillRequestsDismiss() {
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT, "Primary").build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        find(content, R.id.adaptive_ui_sheet_primary_button).performClick();

        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mDismissRequests);
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

        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mDismissRequests);
    }

    @Test
    public void testCloseButtonClick_RequestsDismiss() {
        PropertyModel model =
                newModelBuilder().with(AdaptiveUiProperties.SHOW_CLOSE_BUTTON, true).build();
        AdaptiveUiSheetCoordinator content = createContent(model);

        find(content, R.id.adaptive_ui_sheet_close_button).performClick();

        assertEquals(List.of(DismissReason.CLOSE_BUTTON), mDismissRequests);
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
        assertTrue(mDismissRequests.isEmpty());
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
        assertEquals(List.of(DismissReason.PRIMARY_ACTION), mDismissRequests);
    }

    @Test
    public void testBackPress_UnsetHandlerRequestsDismiss() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());

        content.onBackPressed();

        assertEquals(List.of(DismissReason.BACK_PRESS), mDismissRequests);
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
        assertTrue(mDismissRequests.isEmpty());
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
        assertEquals(List.of(DismissReason.BACK_PRESS), mDismissRequests);
    }

    @Test
    public void testHandleBackPress_ReturnsTrueAndDismisses() {
        AdaptiveUiSheetCoordinator content = createContent(newModelBuilder().build());

        assertTrue(content.handleBackPress());

        assertEquals(List.of(DismissReason.BACK_PRESS), mDismissRequests);
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
}
