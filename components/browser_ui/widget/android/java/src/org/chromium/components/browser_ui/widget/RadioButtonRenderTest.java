// Copyright 2020 The Chromium Authors
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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.List;

/**
 * Render test for {@link RadioButtonWithDescription}, {@link RadioButtonWithEditText}, {@link
 * RadioButtonWithDescriptionAndAuxButton} and {@link RadioButtonWithDescriptionLayout}.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class RadioButtonRenderTest extends BlankUiTestActivityTestCase {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    private static final int REVISION = 3;
    private static final String REVISION_DESCRIPTION =
            "Use Google standard colors as the background.";

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(REVISION)
                    .setDescription(REVISION_DESCRIPTION)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE)
                    .build();

    private RadioButtonWithDescriptionLayout mLayout;

    private RadioButtonWithDescription mRadioButtonWithDescription1;
    private RadioButtonWithDescription mRadioButtonWithDescription2;
    private RadioButtonWithDescription mRadioButtonWithDescription3;

    private RadioButtonWithEditText mRadioButtonWithEditText1;
    private RadioButtonWithEditText mRadioButtonWithEditText2;
    private RadioButtonWithEditText mRadioButtonWithEditText3;
    private RadioButtonWithEditText mRadioButtonWithEditText4;

    private RadioButtonWithDescriptionAndAuxButton mRadioButtonWithDescriptonAndAuxButton1;
    private RadioButtonWithDescriptionAndAuxButton mRadioButtonWithDescriptonAndAuxButton2;
    private RadioButtonWithDescriptionAndAuxButton mRadioButtonWithDescriptonAndAuxButton3;

    private final int mFakeBgColor;

    public RadioButtonRenderTest(boolean nightModeEnabled) {
        mFakeBgColor = nightModeEnabled ? Color.BLACK : Color.WHITE;
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        Activity activity = getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View content =
                            LayoutInflater.from(activity)
                                    .inflate(R.layout.radio_button_render_test, null, false);
                    activity.setContentView(content);

                    mLayout = content.findViewById(R.id.test_radio_button_layout);
                    mLayout.setBackgroundColor(mFakeBgColor);

                    mRadioButtonWithDescription1 = content.findViewById(R.id.base_primary_only);
                    mRadioButtonWithDescription2 =
                            content.findViewById(R.id.base_primary_description);
                    mRadioButtonWithDescription3 =
                            content.findViewById(R.id.base_primary_bg_override);
                    mRadioButtonWithEditText1 =
                            content.findViewById(R.id.edittext_primary_description);
                    mRadioButtonWithEditText2 = content.findViewById(R.id.edittext_primary_only);
                    mRadioButtonWithEditText3 =
                            content.findViewById(R.id.edittext_hint_description);
                    mRadioButtonWithEditText4 = content.findViewById(R.id.edittext_hint_only);
                    mRadioButtonWithDescriptonAndAuxButton1 =
                            content.findViewById(R.id.aux_primary_only);
                    mRadioButtonWithDescriptonAndAuxButton2 =
                            content.findViewById(R.id.aux_primary_description);
                    mRadioButtonWithDescriptonAndAuxButton3 =
                            content.findViewById(R.id.aux_bg_override);
                });

        Assert.assertNotNull(mLayout);
        Assert.assertNotNull(mRadioButtonWithDescription1);
        Assert.assertNotNull(mRadioButtonWithDescription2);
        Assert.assertNotNull(mRadioButtonWithDescription3);
        Assert.assertNotNull(mRadioButtonWithEditText1);
        Assert.assertNotNull(mRadioButtonWithEditText2);
        Assert.assertNotNull(mRadioButtonWithEditText3);
        Assert.assertNotNull(mRadioButtonWithEditText4);
        Assert.assertNotNull(mRadioButtonWithDescriptonAndAuxButton1);
        Assert.assertNotNull(mRadioButtonWithDescriptonAndAuxButton2);
        Assert.assertNotNull(mRadioButtonWithDescriptonAndAuxButton3);
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "RadioButton"})
    public void testRadioButtonWithDescriptionLayout() throws Exception {
        mRenderTestRule.render(mRadioButtonWithDescription1, "base_primary_only");
        mRenderTestRule.render(mRadioButtonWithDescription2, "base_primary_description");
        mRenderTestRule.render(mRadioButtonWithDescription3, "base_primary_bg_override");
        mRenderTestRule.render(mRadioButtonWithEditText1, "edittext_primary_description");
        mRenderTestRule.render(mRadioButtonWithEditText2, "edittext_primary_only");
        mRenderTestRule.render(mRadioButtonWithEditText3, "edittext_hint_description");
        mRenderTestRule.render(mRadioButtonWithEditText4, "edittext_hint_only");
        mRenderTestRule.render(mRadioButtonWithDescriptonAndAuxButton1, "aux_primary_only");
        mRenderTestRule.render(mRadioButtonWithDescriptonAndAuxButton2, "aux_primary_description");
        mRenderTestRule.render(mRadioButtonWithDescriptonAndAuxButton3, "aux_bg_override");
    }
}
