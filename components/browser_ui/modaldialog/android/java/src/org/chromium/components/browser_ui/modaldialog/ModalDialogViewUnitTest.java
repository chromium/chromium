// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import static org.junit.Assert.assertEquals;
import static org.robolectric.Robolectric.buildActivity;

import android.app.Activity;
import android.util.DisplayMetrics;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View.MeasureSpec;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.modaldialog.test.R;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link ModalDialogView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ModalDialogViewUnitTest {
    private static final int MIN_DIALOG_WIDTH = 280;
    private static final int MIN_DIALOG_HEIGHT = 500;
    private static final int MAX_DIALOG_WIDTH_TABLET = 600;
    private static final float MAX_DIALOG_WIDTH_PERCENT_PHONE = 0.65f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private ModalDialogView mDialogView;
    private PropertyModel.Builder mModelBuilder;
    private DisplayMetrics mDisplayMetrics;

    @Before
    public void setup() {
        ModalDialogFeatureMap.setModalDialogLayoutWithSystemInsetsEnabledForTesting(true);
        mActivity = buildActivity(Activity.class).setup().get();
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

    /**
     * Tests that dialog uses a max width of 600dp even if it does not draw into regions beyond
     * margins with the specified width (tablet-only).
     */
    @Test
    @Config(qualifiers = "sw600dp")
    public void measure_SmallDimensions_GreaterThanMaxWidth_Tablet() {
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
                MAX_DIALOG_WIDTH_TABLET + 100,
                MIN_DIALOG_HEIGHT);

        // Measure view.
        var widthMeasureSpec =
                MeasureSpec.makeMeasureSpec(MAX_DIALOG_WIDTH_TABLET + 100, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(MIN_DIALOG_HEIGHT, MeasureSpec.AT_MOST);
        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);

        assertEquals(
                "Width is incorrect.", MAX_DIALOG_WIDTH_TABLET, mDialogView.getMeasuredWidth());
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

    /** Tests that dialog uses a min width of 280dp on tablets. */
    @Test
    @Config(qualifiers = "sw600dp")
    public void measure_SmallDimensions_LessThanMinWidth() {
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
                MIN_DIALOG_WIDTH - 10,
                MIN_DIALOG_HEIGHT);

        // Measure view.
        var widthMeasureSpec = MeasureSpec.makeMeasureSpec(MIN_DIALOG_WIDTH, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(MIN_DIALOG_HEIGHT, MeasureSpec.AT_MOST);
        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);

        assertEquals("Width is incorrect.", MIN_DIALOG_WIDTH, mDialogView.getMeasuredWidth());
        assertEquals("Height is incorrect.", MIN_DIALOG_HEIGHT, mDialogView.getMeasuredHeight());
    }

    /**
     * Tests that dialog uses max size permitted for it to not draw into regions beyond margins on
     * tablets.
     */
    @Test
    @Config(qualifiers = "sw600dp")
    public void measure_LargeDimensions_MarginsSet_Tablet() {
        // Set window size.
        var windowWidth = 600;
        var windowHeight = 600;
        mDisplayMetrics.widthPixels = windowWidth;
        mDisplayMetrics.heightPixels = windowHeight;

        // Create model with margins set.
        createModel(
                mModelBuilder
                        .with(ModalDialogProperties.HORIZONTAL_MARGIN, 16)
                        .with(ModalDialogProperties.VERTICAL_MARGIN, 40),
                windowWidth,
                windowHeight);

        // Measure view.
        var widthMeasureSpec = MeasureSpec.makeMeasureSpec(windowWidth, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(windowHeight, MeasureSpec.AT_MOST);
        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);

        // windowWidth - 2 * horizontalMargin = 600 - 2 * 16.
        int expectedWidth = 568;
        // windowHeight - 2 * verticalMargin = 600 - 2 * 40.
        int expectedHeight = 520;
        assertEquals("Width is incorrect.", expectedWidth, mDialogView.getMeasuredWidth());
        assertEquals("Height is incorrect.", expectedHeight, mDialogView.getMeasuredHeight());
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

    /** Tests that dialog uses specified size if margins are not set on tablets. */
    @Test
    @Config(qualifiers = "sw600dp")
    public void measure_MarginsNotSet_Tablet() {
        // Set window size.
        var windowWidth = 800;
        var windowHeight = 800;
        mDisplayMetrics.widthPixels = windowWidth;
        mDisplayMetrics.heightPixels = windowHeight;

        // Create model without margins set.
        createModel(mModelBuilder, MIN_DIALOG_WIDTH, MIN_DIALOG_HEIGHT);

        // Measure view.
        var widthMeasureSpec =
                MeasureSpec.makeMeasureSpec(MIN_DIALOG_WIDTH + 200, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(MIN_DIALOG_HEIGHT, MeasureSpec.AT_MOST);
        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);

        assertEquals("Width is incorrect.", MIN_DIALOG_WIDTH + 200, mDialogView.getMeasuredWidth());
        assertEquals("Height is incorrect.", MIN_DIALOG_HEIGHT, mDialogView.getMeasuredHeight());
    }

    private void createModel(
            PropertyModel.Builder modelBuilder, int customViewWidth, int customViewHeight) {
        var view = new FrameLayout(mActivity);
        view.setMinimumWidth(customViewWidth);
        view.setMinimumHeight(customViewHeight);
        var model = modelBuilder.with(ModalDialogProperties.CUSTOM_VIEW, view).build();
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());
    }
}
