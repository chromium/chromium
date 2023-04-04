// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.delegate;

import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.app.Activity;
import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup.LayoutParams;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.components.embedder_support.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.List;

/**
 * Render tests for color picker dialog.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@Batch(Batch.UNIT_TESTS)
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
        ColorSuggestion[] suggestions = new ColorSuggestion[8];
        suggestions[0] = new ColorSuggestion(Color.WHITE, "white");
        suggestions[1] = new ColorSuggestion(Color.BLACK, "black");
        suggestions[2] = new ColorSuggestion(Color.YELLOW, "yellow");
        suggestions[3] = new ColorSuggestion(Color.BLUE, "blue");
        suggestions[4] = new ColorSuggestion(Color.GREEN, "green");
        suggestions[5] = new ColorSuggestion(Color.RED, "red");
        suggestions[6] = new ColorSuggestion(Color.MAGENTA, "magenta");
        suggestions[7] = new ColorSuggestion(Color.CYAN, "cyan");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Activity activity = getActivity();
            ColorPickerDialog dialog =
                    new ColorPickerDialog(activity, (v) -> {}, Color.RED, suggestions);
            mView = dialog.getContentView();
            mView.setBackgroundResource(R.color.default_bg_color_baseline);
            activity.setContentView(mView, new LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
        });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_ColorPickerDialog() throws IOException {
        mRenderTestRule.render(mView, "color_picker_dialog");
    }
}
