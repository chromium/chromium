// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

/** Render test for {@link CheckBoxWithDescription}. */
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class CheckBoxWithDescriptionRenderTest extends BlankUiTestActivityTestCase {
    private static final String PRIMARY = "Include all sites under this domain";
    private static final String DESCRIPTION = "Recommended for best experience";
    private static final String LONG_DESCRIPTION =
            "A very very very very very very very very very very very very very long string";

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE_SETTINGS)
                    .build();

    private CheckBoxWithDescription mCheckboxPrimaryOnly;
    private CheckBoxWithDescription mCheckBoxPrimaryDescriptionShort;
    private CheckBoxWithDescription mCheckBoxPrimaryDescriptionLong;
    private CheckBoxWithDescription mCheckboxCheckedPrimaryOnly;
    private CheckBoxWithDescription mCheckBoxCheckedPrimaryDescriptionShort;
    private CheckBoxWithDescription mCheckBoxCheckedPrimaryDescriptionLong;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        Activity activity = getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View content =
                            LayoutInflater.from(activity)
                                    .inflate(
                                            R.layout.checkbox_with_description_render_test,
                                            null,
                                            false);
                    activity.setContentView(content);

                    mCheckboxPrimaryOnly = content.findViewById(R.id.checkbox_primary_only);
                    mCheckBoxPrimaryDescriptionShort =
                            content.findViewById(R.id.checkbox_primary_description_short);
                    mCheckBoxPrimaryDescriptionLong =
                            content.findViewById(R.id.checkbox_primary_description_long);
                    mCheckboxCheckedPrimaryOnly =
                            content.findViewById(R.id.checkbox_checked_primary_only);
                    mCheckBoxCheckedPrimaryDescriptionShort =
                            content.findViewById(R.id.checkbox_checked_primary_description_short);
                    mCheckBoxCheckedPrimaryDescriptionLong =
                            content.findViewById(R.id.checkbox_checked_primary_description_long);

                    mCheckboxPrimaryOnly.setPrimaryText(PRIMARY);
                    mCheckBoxPrimaryDescriptionShort.setPrimaryText(PRIMARY);
                    mCheckBoxPrimaryDescriptionLong.setPrimaryText(PRIMARY);
                    mCheckboxCheckedPrimaryOnly.setPrimaryText(PRIMARY);
                    mCheckBoxCheckedPrimaryDescriptionShort.setPrimaryText(PRIMARY);
                    mCheckBoxCheckedPrimaryDescriptionLong.setPrimaryText(PRIMARY);

                    mCheckBoxPrimaryDescriptionShort.setDescriptionText(DESCRIPTION);
                    mCheckBoxPrimaryDescriptionLong.setDescriptionText(LONG_DESCRIPTION);
                    mCheckBoxCheckedPrimaryDescriptionShort.setDescriptionText(DESCRIPTION);
                    mCheckBoxCheckedPrimaryDescriptionLong.setDescriptionText(LONG_DESCRIPTION);

                    mCheckboxCheckedPrimaryOnly.setChecked(true);
                    mCheckBoxCheckedPrimaryDescriptionShort.setChecked(true);
                    mCheckBoxCheckedPrimaryDescriptionLong.setChecked(true);
                });

        Assert.assertNotNull(mCheckboxPrimaryOnly);
        Assert.assertNotNull(mCheckBoxPrimaryDescriptionShort);
        Assert.assertTrue(mCheckboxCheckedPrimaryOnly.isChecked());
        Assert.assertTrue(mCheckBoxCheckedPrimaryDescriptionShort.isChecked());
        Assert.assertTrue(mCheckBoxCheckedPrimaryDescriptionLong.isChecked());
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testCheckBoxWithDescription() throws Exception {
        mRenderTestRule.render(mCheckboxPrimaryOnly, "checkbox_primary_only");
        mRenderTestRule.render(
                mCheckBoxPrimaryDescriptionShort, "checkbox_primary_description_short");
        mRenderTestRule.render(
                mCheckBoxPrimaryDescriptionLong, "checkbox_primary_description_long");
        mRenderTestRule.render(mCheckboxCheckedPrimaryOnly, "checkbox_checked_primary_only");
        mRenderTestRule.render(
                mCheckBoxCheckedPrimaryDescriptionShort,
                "checkbox_checked_primary_description_short");
        mRenderTestRule.render(
                mCheckBoxCheckedPrimaryDescriptionLong,
                "checkbox_checked_primary_description_long");
    }
}
