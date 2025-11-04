// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.browser_ui.widget.chips;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.app.Activity;
import android.graphics.Color;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.util.Arrays;
import java.util.List;

/**
 * These tests render screenshots of the keyboard accessory bar suggestions and compare them to a
 * gold standard.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class ChipViewRenderTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    // TODO: crbug.com/385172647 - Figure out why night mode test is flaky.
    // TODO: crbug.com/385172647 - Figure out why secondary text doesn't have start padding in
    // the RTL layout.
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false, false).name("Default"),
                    new ParameterSet().value(false, true).name("RTL"));

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE)
                    .setRevision(3)
                    .build();

    private ViewGroup mContentView;

    public ChipViewRenderTest(boolean nightModeEnabled, boolean useRtlLayout) {
        setRtlForTesting(useRtlLayout);
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(useRtlLayout ? "RTL" : "LTR");
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(/* startIntent= */ null);
        Activity activity = mActivityTestRule.getActivity();
        mContentView =
                runOnUiThreadBlocking(
                        () -> {
                            LinearLayout contentView = new LinearLayout(activity);
                            contentView.setOrientation(LinearLayout.VERTICAL);
                            contentView.setBackgroundColor(Color.WHITE);

                            activity.setContentView(
                                    contentView,
                                    new LayoutParams(
                                            LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
                            return contentView;
                        });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void renderSingleLineChip() throws Exception {
        ChipView chip =
                new ChipView(mActivityTestRule.getActivity(), null, 0, R.style.AssistiveChip);
        chip.getPrimaryTextView().setText("Primary text");
        chip.getSecondaryTextView().setText("Secondary text");
        chip.setIconWithTint(R.drawable.ic_settings_gear_24dp, /* tintWithTextColor= */ true);
        chip.addRemoveIcon();

        renderChip(chip, "single_line_chip");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void renderTwoLineChip() throws Exception {
        ChipView chip =
                (ChipView)
                        mActivityTestRule
                                .getActivity()
                                .getLayoutInflater()
                                .inflate(R.layout.two_line_chip_view_test_item, null);
        chip.getPrimaryTextView().setText("Primary text");
        chip.getSecondaryTextView().setText("Secondary text");
        chip.setIconWithTint(R.drawable.ic_settings_gear_24dp, /* tintWithTextColor= */ true);
        chip.addRemoveIcon();

        renderChip(chip, "two_line_chip");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void renderTwoLineChipNoTintedIcon() throws Exception {
        ChipView chip =
                (ChipView)
                        mActivityTestRule
                                .getActivity()
                                .getLayoutInflater()
                                .inflate(R.layout.two_line_chip_view_test_item, null);
        chip.getPrimaryTextView().setText("Primary text");
        chip.getSecondaryTextView().setText("Secondary text");
        // Do not reset the icon's tint and make sure the test renders a white image.
        chip.setIconWithTint(R.drawable.ic_settings_gear_24dp, /* tintWithTextColor= */ false);
        chip.addRemoveIcon();

        renderChip(chip, "two_line_chip_no_tinted_icon");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void renderSingleLineChipWithReducedWidth() throws Exception {
        ChipView chip =
                new ChipView(mActivityTestRule.getActivity(), null, 0, R.style.AssistiveChip);
        chip.getPrimaryTextView().setText("Primary looooooooong text");
        chip.getSecondaryTextView().setText("S. t.");
        chip.setIconWithTint(R.drawable.ic_settings_gear_24dp, /* tintWithTextColor= */ true);
        chip.addRemoveIcon();
        reduceChipWidth(chip);

        renderChip(chip, "single_line_chip_with_reduced_width");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void renderTwoLineChipWithReducedWidth() throws Exception {
        ChipView chip =
                (ChipView)
                        mActivityTestRule
                                .getActivity()
                                .getLayoutInflater()
                                .inflate(R.layout.two_line_chip_view_test_item, null);
        chip.getPrimaryTextView().setText("Primary loooooooong text");
        chip.getSecondaryTextView().setText("S. t. ");
        chip.setIconWithTint(R.drawable.ic_settings_gear_24dp, /* tintWithTextColor= */ true);
        chip.addRemoveIcon();
        reduceChipWidth(chip);

        renderChip(chip, "two_line_chip_with_reduced_width");
    }

    private void renderChip(ChipView chipView, String name) throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    mContentView.addView(
                            chipView,
                            new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
                });

        mRenderTestRule.render(mContentView, name);
    }

    private void reduceChipWidth(ChipView chip) {
        runOnUiThreadBlocking(
                () -> {
                    chip.setMaxWidth(500);
                });
    }
}
