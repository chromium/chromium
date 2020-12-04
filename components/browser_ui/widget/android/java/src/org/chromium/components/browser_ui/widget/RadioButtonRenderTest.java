// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.app.Activity;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.List;

/**
 * Render test for {@link RadioButtonWithDescription}, {@link RadioButtonWithEditText} and
 * {@link RadioButtonWithDescriptionLayout}.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
public class RadioButtonRenderTest extends DummyUiActivityTestCase {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    private static final int REVISION = 1;
    private static final String REVISION_DESCRIPTION = "Updated EditText hint color for a11y";

    @Rule
    public RenderTestRule mRenderTestRule = RenderTestRule.Builder.withPublicCorpus()
                                                    .setRevision(REVISION)
                                                    .setDescription(REVISION_DESCRIPTION)
                                                    .build();

    private RadioButtonWithDescriptionLayout mLayout;

    private RadioButtonWithDescription mRadioButtonWithDescription1;
    private RadioButtonWithDescription mRadioButtonWithDescription2;
    private RadioButtonWithDescription mRadioButtonWithDescription3;

    private RadioButtonWithEditText mRadioButtonWithEditText1;
    private RadioButtonWithEditText mRadioButtonWithEditText2;
    private RadioButtonWithEditText mRadioButtonWithEditText3;
    private RadioButtonWithEditText mRadioButtonWithEditText4;

    private final int mFakeBgColor;

    public RadioButtonRenderTest(boolean nightModeEnabled) {
        mFakeBgColor = nightModeEnabled ? Color.BLACK : Color.WHITE;
        NightModeTestUtils.setUpNightModeForDummyUiActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        Activity activity = getActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            View content = LayoutInflater.from(activity).inflate(
                    R.layout.radio_button_render_test, null, false);
            activity.setContentView(content);

            mLayout = content.findViewById(R.id.test_radio_button_layout);
            mLayout.setBackgroundColor(mFakeBgColor);

            mRadioButtonWithDescription1 = content.findViewById(R.id.test_radio_description_1);
            mRadioButtonWithDescription2 = content.findViewById(R.id.test_radio_description_2);
            mRadioButtonWithDescription3 = content.findViewById(R.id.test_radio_description_3);
            mRadioButtonWithEditText1 = content.findViewById(R.id.test_radio_edit_text_1);
            mRadioButtonWithEditText2 = content.findViewById(R.id.test_radio_edit_text_2);
            mRadioButtonWithEditText3 = content.findViewById(R.id.test_radio_edit_text_3);
            mRadioButtonWithEditText4 = content.findViewById(R.id.test_radio_edit_text_4);
        });

        Assert.assertNotNull(mLayout);
        Assert.assertNotNull(mRadioButtonWithDescription1);
        Assert.assertNotNull(mRadioButtonWithDescription2);
        Assert.assertNotNull(mRadioButtonWithDescription3);
        Assert.assertNotNull(mRadioButtonWithEditText1);
        Assert.assertNotNull(mRadioButtonWithEditText2);
        Assert.assertNotNull(mRadioButtonWithEditText3);
        Assert.assertNotNull(mRadioButtonWithEditText4);
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "RadioButton"})
    public void testRadioButtonWithDescriptionLayout() throws Exception {
        mRenderTestRule.render(mRadioButtonWithDescription1, "test_radio_description_1");
        mRenderTestRule.render(mRadioButtonWithDescription2, "test_radio_description_2");
        mRenderTestRule.render(mRadioButtonWithDescription3, "test_radio_description_3");
        mRenderTestRule.render(mRadioButtonWithEditText1, "test_radio_edit_text_1");
        mRenderTestRule.render(mRadioButtonWithEditText2, "test_radio_edit_text_2");
        mRenderTestRule.render(mRadioButtonWithEditText3, "test_radio_edit_text_3");
        mRenderTestRule.render(mRadioButtonWithEditText4, "test_radio_edit_text_4");
    }
}
