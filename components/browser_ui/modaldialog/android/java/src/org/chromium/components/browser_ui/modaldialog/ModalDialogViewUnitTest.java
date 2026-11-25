// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.robolectric.Robolectric.buildActivity;

import android.app.Activity;
import android.graphics.Rect;
import android.util.DisplayMetrics;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.time.Duration;

/** Unit tests for {@link ModalDialogView}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ModalDialogFeatureList.MODAL_DIALOG_LAYOUT_WITH_SYSTEM_INSETS)
public class ModalDialogViewUnitTest {
    private static final int MIN_DIALOG_WIDTH = 280;
    private static final int MIN_DIALOG_HEIGHT = 500;
    private static final int MAX_DIALOG_WIDTH_LFF = 480;
    private static final float MAX_DIALOG_WIDTH_PERCENT_PHONE = 0.65f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private ModalDialogView mDialogView;
    private PropertyModel.Builder mModelBuilder;
    private DisplayMetrics mDisplayMetrics;

    @Before
    public void setup() {
        mActivity = buildActivity(TestActivity.class).setup().get();
        mDialogView =
                (ModalDialogView)
                        LayoutInflater.from(
                                        new ContextThemeWrapper(
                                                mActivity,
                                                R.style
                                                        .ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton))
                                .inflate(R.layout.modal_dialog_view, null);
        mModelBuilder = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS);
        mDisplayMetrics = mActivity.getResources().getDisplayMetrics();
        mDisplayMetrics.density = 1;
    }

    /** Tests that dialog uses specified size if it does not draw into regions beyond margins. */
    @Test
    public void measure_SmallDimensions_LessThanMaxWidth() {
        // Set window size.
        var windowWidth = 800;
        var windowHeight = 800;
        mDisplayMetrics.widthPixels = windowWidth;
        mDisplayMetrics.heightPixels = windowHeight;

        // Create model with margins set.
        createModel(
                mModelBuilder
                        .with(ModalDialogProperties.HORIZONTAL_MARGIN, 10)
                        .with(ModalDialogProperties.VERTICAL_MARGIN, 40),
                MIN_DIALOG_WIDTH,
                MIN_DIALOG_HEIGHT);

        // Measure view.
        var widthMeasureSpec = MeasureSpec.makeMeasureSpec(MIN_DIALOG_WIDTH, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(MIN_DIALOG_HEIGHT, MeasureSpec.AT_MOST);
        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);

        assertEquals("Width is incorrect.", MIN_DIALOG_WIDTH, mDialogView.getMeasuredWidth());
        assertEquals("Height is incorrect.", MIN_DIALOG_HEIGHT, mDialogView.getMeasuredHeight());
    }

    /** Tests that dialog uses a max width of (65% * window width) in landscape on phones. */
    @Test
    @Config(qualifiers = "sw320dp-land")
    public void measure_SmallDimensions_GreaterThanMaxWidth_Phone() {
        // Set window size.
        var windowWidth = 100;
        var windowHeight = 100;
        mDisplayMetrics.widthPixels = windowWidth;
        mDisplayMetrics.heightPixels = windowHeight;

        int maxDialogWidthPhone = (int) (MAX_DIALOG_WIDTH_PERCENT_PHONE * windowWidth);

        // Create model.
        createModel(mModelBuilder, maxDialogWidthPhone + 20, MIN_DIALOG_HEIGHT);

        // Measure view.
        var widthMeasureSpec =
                MeasureSpec.makeMeasureSpec(maxDialogWidthPhone + 20, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(MIN_DIALOG_HEIGHT, MeasureSpec.AT_MOST);
        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);

        assertEquals("Width is incorrect.", maxDialogWidthPhone, mDialogView.getMeasuredWidth());
        assertEquals("Height is incorrect.", MIN_DIALOG_HEIGHT, mDialogView.getMeasuredHeight());
    }

    /** Tests that dialog maintains horizontal margin from the edges on phones. */
    @Test
    @Config(qualifiers = "sw320dp")
    public void measure_LargeDimensions_MarginsSet_Phone() {
        // Set window size.
        var windowWidth = 80;
        var windowHeight = 80;
        mDisplayMetrics.widthPixels = windowWidth;
        mDisplayMetrics.heightPixels = windowHeight;

        // Create model with margins set.
        createModel(
                mModelBuilder.with(ModalDialogProperties.HORIZONTAL_MARGIN, 16),
                windowWidth,
                windowHeight);

        // Measure view.
        var widthMeasureSpec = MeasureSpec.makeMeasureSpec(windowWidth, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(windowHeight, MeasureSpec.AT_MOST);
        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);

        // windowWidth - 2 * horizontalMargin = 80 - 2 * 16.
        int expectedWidth = 48;
        // windowHeight - 2 * verticalMargin = 80 - 2 * 0.
        int expectedHeight = 80;
        assertEquals("Width is incorrect.", expectedWidth, mDialogView.getMeasuredWidth());
        assertEquals("Height is incorrect.", expectedHeight, mDialogView.getMeasuredHeight());
    }

    /** Tests that dialog uses specified size when margins are not set on phones. */
    @Test
    @Config(qualifiers = "sw320dp")
    public void measure_MarginsNotSet_Phone() {
        // Set window size.
        var windowWidth = 800;
        var windowHeight = 800;
        mDisplayMetrics.widthPixels = windowWidth;
        mDisplayMetrics.heightPixels = windowHeight;

        // Create model without margins set.
        createModel(mModelBuilder, 500, 500);

        // Measure view.
        var widthMeasureSpec = MeasureSpec.makeMeasureSpec(windowWidth, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(windowHeight, MeasureSpec.AT_MOST);
        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);

        assertEquals("Width is incorrect.", 500, mDialogView.getMeasuredWidth());
        assertEquals("Height is incorrect.", 500, mDialogView.getMeasuredHeight());
    }

    /**
     * Tests that a vertical margin alone does not constrain the width. Bounding the height must not
     * turn the width spec into EXACTLY, or the dialog fills the window instead of wrapping.
     */
    @Test
    @Config(qualifiers = "sw320dp")
    public void measure_VerticalMarginOnly_Phone_DoesNotForceWidth() {
        var windowWidth = 800;
        var windowHeight = 800;
        mDisplayMetrics.widthPixels = windowWidth;
        mDisplayMetrics.heightPixels = windowHeight;

        createModel(mModelBuilder.with(ModalDialogProperties.VERTICAL_MARGIN, 24), 500, 500);

        var widthMeasureSpec = MeasureSpec.makeMeasureSpec(windowWidth, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(windowHeight, MeasureSpec.AT_MOST);
        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);

        assertEquals(
                "Dialog should wrap its content, not fill the window.",
                500,
                mDialogView.getMeasuredWidth());
        assertEquals("Height is incorrect.", 500, mDialogView.getMeasuredHeight());
    }

    private void createModel(
            PropertyModel.Builder modelBuilder, int customViewWidth, int customViewHeight) {
        var view = new FrameLayout(mActivity);
        view.setMinimumWidth(customViewWidth);
        view.setMinimumHeight(customViewHeight);
        var model = modelBuilder.with(ModalDialogProperties.CUSTOM_VIEW, view).build();
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());
    }

    @Test
    public void testBottomSpacerVisibility_WithLargePadding() {
        // Create model with no buttons but large bottom padding.
        mModelBuilder.with(ModalDialogProperties.PADDING, new Rect(0, 0, 0, 20));
        createModel(mModelBuilder, MIN_DIALOG_WIDTH, MIN_DIALOG_HEIGHT);

        android.view.View spacer = mDialogView.findViewById(R.id.dialog_bottom_spacer);
        assertEquals(
                "Spacer should be gone when large padding is present.",
                android.view.View.GONE,
                spacer.getVisibility());
    }

    @Test
    public void testBottomSpacerVisibility_ButtonsPresent() {
        var model =
                mModelBuilder
                        .with(ModalDialogProperties.TITLE, "Title")
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, "OK")
                        .build();
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());

        android.view.View spacer = mDialogView.findViewById(R.id.dialog_bottom_spacer);
        assertEquals(
                "Spacer should be GONE when buttons are present.",
                android.view.View.GONE,
                spacer.getVisibility());
    }

    @Test
    public void testBottomSpacerVisibility_CustomButtonBarPresent() {
        var customButtonBar = new FrameLayout(mActivity);
        var model =
                mModelBuilder
                        .with(ModalDialogProperties.TITLE, "Title")
                        .with(ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW, customButtonBar)
                        .build();
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());

        android.view.View spacer = mDialogView.findViewById(R.id.dialog_bottom_spacer);
        assertEquals(
                "Spacer should be GONE when custom button bar is present.",
                android.view.View.GONE,
                spacer.getVisibility());
    }

    @Test
    public void testBottomSpacerVisibility_CustomViewAtBottom() {
        var customView = new FrameLayout(mActivity);
        var model =
                mModelBuilder
                        .with(ModalDialogProperties.TITLE, "Title")
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .build();
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());

        android.view.View spacer = mDialogView.findViewById(R.id.dialog_bottom_spacer);
        assertEquals(
                "Spacer should be GONE when custom view is at the bottom.",
                android.view.View.GONE,
                spacer.getVisibility());
    }

    @Test
    public void testBottomSpacerVisibility_FooterVisible() {
        var model =
                mModelBuilder
                        .with(ModalDialogProperties.TITLE, "Title")
                        .with(ModalDialogProperties.FOOTER_MESSAGE, "Footer")
                        .build();
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());

        android.view.View spacer = mDialogView.findViewById(R.id.dialog_bottom_spacer);
        assertEquals(
                "Spacer should be GONE when footer is visible.",
                android.view.View.GONE,
                spacer.getVisibility());
    }

    @Test
    public void testBottomSpacerVisibility_CheckboxBelowCustomView() {
        var customView = new FrameLayout(mActivity);
        var model =
                mModelBuilder
                        .with(ModalDialogProperties.TITLE, "Title")
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(ModalDialogProperties.CHECKBOX_TEXT, "Checkbox")
                        .build();
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());

        android.view.View spacer = mDialogView.findViewById(R.id.dialog_bottom_spacer);
        assertEquals(
                "Spacer should be VISIBLE when checkbox is below a custom view.",
                android.view.View.VISIBLE,
                spacer.getVisibility());
    }

    @Test
    public void testWrapCustomViewInScrollable_SetAfterBind() {
        var customView = new FrameLayout(mActivity);
        var model = mModelBuilder.with(ModalDialogProperties.CUSTOM_VIEW, customView).build();
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());
        mActivity.setContentView(mDialogView);

        // The initial bind order isn't defined, so set the key afterwards to cover the case where
        // it's bound last.
        model.set(ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE, true);

        assertTrue("The custom view should be shown.", customView.isShown());
    }

    @Test
    public void testWrapCustomViewInScrollable_UnsetAfterBind() {
        // Embedded permission prompts unset the wrap at runtime, which needs
        // CHANGE_CUSTOM_VIEW_OR_BUTTONS.
        var model =
                mModelBuilder
                        .with(ModalDialogProperties.CHANGE_CUSTOM_VIEW_OR_BUTTONS, true)
                        .with(ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE, true)
                        .build();
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());
        // Set the custom view after the initial bind, so the scroll view is shown before the
        // change whatever the bind order.
        model.set(ModalDialogProperties.CUSTOM_VIEW, new FrameLayout(mActivity));

        model.set(ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE, false);

        assertEquals(
                "The scroll view should be hidden once it no longer holds the custom view.",
                View.GONE,
                mDialogView.findViewById(R.id.modal_dialog_scroll_view).getVisibility());
    }

    @Test
    public void testWrapCustomViewInScrollable_NotFocusableWhenCustomViewFits() {
        var shortCustomView = new FrameLayout(mActivity);
        shortCustomView.setMinimumHeight(MIN_DIALOG_HEIGHT / 2);
        var model =
                mModelBuilder
                        .with(ModalDialogProperties.CUSTOM_VIEW, shortCustomView)
                        .with(ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE, true)
                        .build();
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());

        var widthMeasureSpec = MeasureSpec.makeMeasureSpec(MIN_DIALOG_WIDTH, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(MIN_DIALOG_HEIGHT, MeasureSpec.AT_MOST);
        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);
        mDialogView.layout(0, 0, mDialogView.getMeasuredWidth(), mDialogView.getMeasuredHeight());

        View scrollView = mDialogView.findViewById(R.id.modal_dialog_scroll_view);
        assertFalse(scrollView.canScrollVertically(-1));
        assertFalse(scrollView.canScrollVertically(1));
        assertFalse(scrollView.isFocusable());
    }

    @Test
    public void testWrapCustomViewInScrollable_FocusableWhenCustomViewScrolls() {
        var tallCustomView = new FrameLayout(mActivity);
        tallCustomView.setMinimumHeight(MIN_DIALOG_HEIGHT * 2);
        var model =
                mModelBuilder
                        .with(ModalDialogProperties.CUSTOM_VIEW, tallCustomView)
                        .with(ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE, true)
                        .build();
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());

        // ScrollView starts focusable. Clear it so the test fails if layout doesn't set it back.
        View scrollView = mDialogView.findViewById(R.id.modal_dialog_scroll_view);
        scrollView.setFocusable(false);

        var widthMeasureSpec = MeasureSpec.makeMeasureSpec(MIN_DIALOG_WIDTH, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(MIN_DIALOG_HEIGHT, MeasureSpec.AT_MOST);
        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);
        mDialogView.layout(0, 0, mDialogView.getMeasuredWidth(), mDialogView.getMeasuredHeight());

        assertTrue(scrollView.canScrollVertically(1));
        assertTrue(scrollView.isFocusable());
    }

    @Test
    public void testButtonTapProtection_AttachedWithoutAnimation() {
        class TestController implements ModalDialogProperties.Controller {
            int mClickedButton = -1;

            @Override
            public void onClick(PropertyModel model, int buttonType) {
                mClickedButton = buttonType;
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {}
        }
        var controller = new TestController();

        var model =
                mModelBuilder
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, "OK")
                        .with(ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, 1000L)
                        .build();
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());

        // Attach to window.
        mActivity.setContentView(mDialogView);

        var positiveButton = mDialogView.findViewById(R.id.positive_button);

        // Click immediately - should be blocked.
        positiveButton.performClick();
        assertEquals(-1, controller.mClickedButton);

        // Advance time by 500ms (less than 1000ms).
        ShadowSystemClock.advanceBy(Duration.ofMillis(500));
        positiveButton.performClick();
        assertEquals(-1, controller.mClickedButton);

        // Advance time by 1100ms (more than 1000ms since last click).
        ShadowSystemClock.advanceBy(Duration.ofMillis(1100));
        positiveButton.performClick();
        assertEquals(ModalDialogProperties.ButtonType.POSITIVE, controller.mClickedButton);
    }

    @Test
    public void testButtonTapProtection_Restarted() {
        class TestController implements ModalDialogProperties.Controller {
            int mClickedButton = -1;

            @Override
            public void onClick(PropertyModel model, int buttonType) {
                mClickedButton = buttonType;
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {}
        }
        var controller = new TestController();

        var model =
                mModelBuilder
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, "OK")
                        .with(ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, 1000L)
                        .build();
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());

        // Restarting before the view is attached is a no-op; protection starts once attached.
        mDialogView.replayButtonTapProtection();
        ShadowSystemClock.advanceBy(Duration.ofMillis(1100));

        // Attach to window.
        mActivity.setContentView(mDialogView);
        var positiveButton = mDialogView.findViewById(R.id.positive_button);

        // Click right after attaching - should be blocked since protection starts on attach, not
        // on the earlier replay call.
        positiveButton.performClick();
        assertEquals(-1, controller.mClickedButton);

        // The protection period elapses while the dialog is not visible to the user yet (e.g.
        // during the hosting activity's enter animation).
        ShadowSystemClock.advanceBy(Duration.ofMillis(1100));

        // The dialog becomes visible and the protection period is restarted.
        mDialogView.replayButtonTapProtection();

        // Click right after becoming visible - should be blocked.
        ShadowSystemClock.advanceBy(Duration.ofMillis(10));
        positiveButton.performClick();
        assertEquals(-1, controller.mClickedButton);

        // Advance time by 1100ms (more than 1000ms since last click).
        ShadowSystemClock.advanceBy(Duration.ofMillis(1100));
        positiveButton.performClick();
        assertEquals(ModalDialogProperties.ButtonType.POSITIVE, controller.mClickedButton);
    }

    @Test
    public void testButtonTapProtection_RestartDoesNotShortenScheduledPeriod() {
        class TestController implements ModalDialogProperties.Controller {
            int mClickedButton = -1;

            @Override
            public void onClick(PropertyModel model, int buttonType) {
                mClickedButton = buttonType;
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {}
        }
        var controller = new TestController();

        var model =
                mModelBuilder
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, "OK")
                        .with(ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, 1000L)
                        .build();
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());
        mActivity.setContentView(mDialogView);
        var positiveButton = mDialogView.findViewById(R.id.positive_button);

        // Protection is scheduled to start 1000ms from now (in the middle of the animation).
        mDialogView.onEnterAnimationStarted(2000);
        mDialogView.replayButtonTapProtection();

        // 1500ms from now is still within 1000ms of the scheduled start - should be blocked.
        ShadowSystemClock.advanceBy(Duration.ofMillis(1500));
        positiveButton.performClick();
        assertEquals(-1, controller.mClickedButton);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void measure_LargeFormFactorUi_Tablet_WidthCappedAt480dp() {
        // Wide tablet window (800x800).
        mDisplayMetrics.widthPixels = 800;
        mDisplayMetrics.heightPixels = 800;

        createModel(mModelBuilder, 600, MIN_DIALOG_HEIGHT);

        var widthMeasureSpec = MeasureSpec.makeMeasureSpec(600, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(MIN_DIALOG_HEIGHT, MeasureSpec.AT_MOST);
        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);

        assertEquals(
                "Width should be capped at 480dp on large form factor.",
                MAX_DIALOG_WIDTH_LFF,
                mDialogView.getMeasuredWidth());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void measure_LargeFormFactorUi_WindowOwningDialog_NarrowWindow_KeepsHorizontalMargin() {
        // Narrow app window (400dp < 480dp), with the margin its presenter supplies.
        var windowWidth = 400;
        mDisplayMetrics.widthPixels = windowWidth;
        mDisplayMetrics.heightPixels = 800;

        createModel(
                mModelBuilder.with(ModalDialogProperties.HORIZONTAL_MARGIN, 16),
                600,
                MIN_DIALOG_HEIGHT);

        var widthMeasureSpec = MeasureSpec.makeMeasureSpec(windowWidth, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(MIN_DIALOG_HEIGHT, MeasureSpec.AT_MOST);
        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);

        // Expected width = windowWidth - 2 * 16dp = 400 - 32 = 368dp.
        int expectedWidth = windowWidth - 2 * 16;
        assertEquals(
                "Width should maintain at least 16dp horizontal margin.",
                expectedWidth,
                mDialogView.getMeasuredWidth());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void measure_LargeFormFactorUi_ContainerHostedDialog_FillsOfferedWidth() {
        // Side UI constrains the container to 360dp. Its presenter gives the dialog real layout
        // margins, so the 360dp is already 2 * 16dp short by the time it is offered here.
        mDisplayMetrics.widthPixels = 1000;
        mDisplayMetrics.heightPixels = 800;

        int offeredWidth = 360 - 2 * 16;
        createModel(mModelBuilder, 600, MIN_DIALOG_HEIGHT);

        var widthMeasureSpec = MeasureSpec.makeMeasureSpec(offeredWidth, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(MIN_DIALOG_HEIGHT, MeasureSpec.AT_MOST);
        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);

        assertEquals(
                "Width should fill the space the container offers, which already excludes the"
                        + " dialog's margins.",
                offeredWidth,
                mDialogView.getMeasuredWidth());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void measure_LargeFormFactorUi_WindowOwningDialog_ShortWindow_KeepsVerticalMargin() {
        // Short window (500dp tall).
        var containerHeight = 500;
        mDisplayMetrics.heightPixels = containerHeight;

        // Content requests 600dp height.
        createModel(
                mModelBuilder.with(ModalDialogProperties.VERTICAL_MARGIN, 24),
                MAX_DIALOG_WIDTH_LFF,
                600);

        var widthMeasureSpec =
                MeasureSpec.makeMeasureSpec(MAX_DIALOG_WIDTH_LFF, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(containerHeight, MeasureSpec.AT_MOST);
        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);

        // Expected height = containerHeight - 2 * 24dp = 500 - 48 = 452dp.
        int expectedHeight = containerHeight - 2 * 24;
        assertEquals(
                "Height should maintain at least 24dp vertical margin.",
                expectedHeight,
                mDialogView.getMeasuredHeight());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void measure_LargeFormFactorUi_Desktop_WidthCappedAt480dp() {
        DeviceInfo.setIsDesktopForTesting(true);

        mDisplayMetrics.widthPixels = 1200;
        mDisplayMetrics.heightPixels = 800;

        createModel(mModelBuilder, 600, MIN_DIALOG_HEIGHT);

        var widthMeasureSpec = MeasureSpec.makeMeasureSpec(600, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(MIN_DIALOG_HEIGHT, MeasureSpec.AT_MOST);
        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);

        assertEquals(
                "Width should be capped at 480dp on desktop.",
                MAX_DIALOG_WIDTH_LFF,
                mDialogView.getMeasuredWidth());
    }

    /**
     * ViewRootImpl re-measures a window-owning dialog against the frame the previous pass produced,
     * so measurement has to reach a fixed point. See crbug.com/557352977.
     */
    @Test
    @Config(qualifiers = "sw600dp")
    public void measure_LargeFormFactorUi_WindowOwningDialog_ReachesFixedPoint() {
        mDisplayMetrics.widthPixels = 1600;
        mDisplayMetrics.heightPixels = 1200;

        createModel(
                mModelBuilder
                        .with(ModalDialogProperties.HORIZONTAL_MARGIN, 16)
                        .with(ModalDialogProperties.VERTICAL_MARGIN, 24),
                600,
                MIN_DIALOG_HEIGHT);

        // Later passes feed back the size this view reported.
        int width = 1160;
        int height = 1000;
        for (int pass = 0; pass < 4; pass++) {
            mDialogView.measure(
                    MeasureSpec.makeMeasureSpec(width, MeasureSpec.AT_MOST),
                    MeasureSpec.makeMeasureSpec(height, MeasureSpec.AT_MOST));
            int measuredWidth = mDialogView.getMeasuredWidth();
            int measuredHeight = mDialogView.getMeasuredHeight();
            if (pass > 0) {
                assertEquals(
                        "Width must not change when re-measured against its own previous size.",
                        width,
                        measuredWidth);
                assertEquals(
                        "Height must not change when re-measured against its own previous size.",
                        height,
                        measuredHeight);
            }
            width = measuredWidth;
            height = measuredHeight;
        }
    }

    /** A container-hosted dialog is measured against stable container bounds every pass. */
    @Test
    @Config(qualifiers = "sw600dp")
    public void measure_LargeFormFactorUi_ContainerHostedDialog_ReachesFixedPoint() {
        mDisplayMetrics.widthPixels = 1600;
        mDisplayMetrics.heightPixels = 1200;

        createModel(mModelBuilder, 600, MIN_DIALOG_HEIGHT);

        int containerWidth = 800;
        int containerHeight = 700;
        var widthMeasureSpec = MeasureSpec.makeMeasureSpec(containerWidth, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(containerHeight, MeasureSpec.AT_MOST);

        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);
        int firstWidth = mDialogView.getMeasuredWidth();
        int firstHeight = mDialogView.getMeasuredHeight();

        for (int pass = 0; pass < 3; pass++) {
            mDialogView.measure(widthMeasureSpec, heightMeasureSpec);
            assertEquals(
                    "Width must be stable across repeated measures.",
                    firstWidth,
                    mDialogView.getMeasuredWidth());
            assertEquals(
                    "Height must be stable across repeated measures.",
                    firstHeight,
                    mDialogView.getMeasuredHeight());
        }
    }

    @Test
    public void testCloseButtonStaysAtEndWithoutTitle() {
        var model =
                mModelBuilder.with(ModalDialogProperties.TITLE_CLOSE_BUTTON_VISIBLE, true).build();
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());

        mDialogView.measure(
                MeasureSpec.makeMeasureSpec(MAX_DIALOG_WIDTH_LFF, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(MIN_DIALOG_HEIGHT, MeasureSpec.AT_MOST));
        mDialogView.layout(0, 0, mDialogView.getMeasuredWidth(), mDialogView.getMeasuredHeight());

        View titleRow = mDialogView.findViewById(R.id.title_container);
        View closeButton = titleRow.findViewById(R.id.title_close_button);
        MarginLayoutParams params = (MarginLayoutParams) closeButton.getLayoutParams();
        assertEquals(
                "The close button should be at the end of the title row.",
                titleRow.getWidth() - titleRow.getPaddingRight() - params.rightMargin,
                closeButton.getRight());
    }
}
