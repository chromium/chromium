// Copyright 2021 The Chromium Authors
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

/** Render test for {@link RadioButtonWithDescription} with the icon. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@Batch(Batch.UNIT_TESTS)
public class RadioButtonWithIconRenderTest extends BlankUiTestActivityTestCase {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    private static final int REVISION = 3;
    private static final String REVISION_DESCRIPTION =
            "Test rendering of the icon in a disabled state.";

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(REVISION)
                    .setDescription(REVISION_DESCRIPTION)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE)
                    .build();

    private RadioButtonWithDescriptionLayout mLayout;

    private RadioButtonWithDescription mRadioButtonWithIcon1;
    private RadioButtonWithDescription mRadioButtonWithIcon2;
    private RadioButtonWithDescription mRadioButtonWithIcon3;
    private RadioButtonWithDescription mRadioButtonWithIcon4;

    private final int mFakeBgColor;

    public RadioButtonWithIconRenderTest(boolean nightModeEnabled) {
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
                                    .inflate(
                                            R.layout.radio_button_with_icon_render_test,
                                            null,
                                            false);
                    activity.setContentView(content);

                    mLayout = content.findViewById(R.id.test_radio_button_layout);
                    mLayout.setBackgroundColor(mFakeBgColor);

                    mRadioButtonWithIcon1 = content.findViewById(R.id.icon_primary_only);
                    mRadioButtonWithIcon2 = content.findViewById(R.id.icon_primary_description);
                    mRadioButtonWithIcon3 = content.findViewById(R.id.icon_bg_override);
                    mRadioButtonWithIcon4 = content.findViewById(R.id.icon_disabled);
                });

        Assert.assertNotNull(mLayout);
        Assert.assertNotNull(mRadioButtonWithIcon1);
        Assert.assertNotNull(mRadioButtonWithIcon2);
        Assert.assertNotNull(mRadioButtonWithIcon3);
        Assert.assertNotNull(mRadioButtonWithIcon4);
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "RadioButton"})
    public void testRadioButtonWithIcon() throws Exception {
        mRenderTestRule.render(mRadioButtonWithIcon1, "icon_primary_only");
        mRenderTestRule.render(mRadioButtonWithIcon2, "icon_primary_description");
        mRenderTestRule.render(mRadioButtonWithIcon3, "icon_bg_override");
        mRenderTestRule.render(mRadioButtonWithIcon4, "icon_disabled");
    }
}
