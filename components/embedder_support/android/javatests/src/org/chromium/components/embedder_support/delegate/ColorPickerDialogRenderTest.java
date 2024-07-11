// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.delegate;

import android.app.Activity;
import android.graphics.Color;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.embedder_support.R;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.List;

/** Render tests for color picker dialog. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
// TODO(crbug.com/344923212): Failing when batched, batch this again.
public class ColorPickerDialogRenderTest extends BlankUiTestActivityTestCase {

    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.BLINK_FORMS_COLOR)
                    .build();

    private View mView;

    public ColorPickerDialogRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Activity activity = getActivity();
                    ColorPickerDialogView dialog = new ColorPickerDialogView(activity);
                    ColorPickerCoordinator mColorPickerCoordinator =
                            new ColorPickerCoordinator(activity, (i) -> {}, dialog);
                    mView = dialog.getContentView();
                    mView.setBackgroundResource(R.color.default_bg_color_baseline);
                    mColorPickerCoordinator.show(Color.RED);
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_ColorPickerDialog() throws IOException {
        mRenderTestRule.render(mView, "color_picker_dialog");
    }
}
