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
    private static final int MAX_DIALOG_WIDTH = 600;

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

    /** Tests that dialog uses specified size if it does not draw into insets' regions. */
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
     * Tests that dialog uses a max width of 600dp even if it does not draw into insets' regions
     * with the specified width.
     */
    @Test
    public void measure_SmallDimensions_GreaterThanMaxWidth() {
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
                MAX_DIALOG_WIDTH + 100,
                MIN_DIALOG_HEIGHT);

        // Measure view.
        var widthMeasureSpec =
                MeasureSpec.makeMeasureSpec(MAX_DIALOG_WIDTH + 100, MeasureSpec.AT_MOST);
        var heightMeasureSpec = MeasureSpec.makeMeasureSpec(MIN_DIALOG_HEIGHT, MeasureSpec.AT_MOST);
        mDialogView.measure(widthMeasureSpec, heightMeasureSpec);

        assertEquals("Width is incorrect.", MAX_DIALOG_WIDTH, mDialogView.getMeasuredWidth());
        assertEquals("Height is incorrect.", MIN_DIALOG_HEIGHT, mDialogView.getMeasuredHeight());
    }

    /**
     * Tests that dialog uses max size permitted for it to not draw into insets' regions when
     * margins are set.
     */
    @Test
    public void measure_LargeDimensions_MarginsSet() {
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

    /** Tests that dialog uses specified size when margins are not set. */
    @Test
    public void measure_MarginsNotSet() {
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

    private void createModel(
            PropertyModel.Builder modelBuilder, int customViewWidth, int customViewHeight) {
        var view = new FrameLayout(mActivity);
        view.setMinimumWidth(customViewWidth);
        view.setMinimumHeight(customViewHeight);
        var model = modelBuilder.with(ModalDialogProperties.CUSTOM_VIEW, view).build();
        PropertyModelChangeProcessor.create(model, mDialogView, new ModalDialogViewBinder());
    }
}
