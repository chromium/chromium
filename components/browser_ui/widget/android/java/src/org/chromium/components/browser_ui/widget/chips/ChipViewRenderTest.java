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
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false, false).name("Default"),
                    new ParameterSet().value(false, true).name("RTL"));

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE)
                    .setRevision(2)
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
                                            LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
                            return contentView;
                        });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void renderSuggestions() throws Exception {
        // All suggestion types are rendered in the same test to minimize the number of render
        // tests.
        // TODO: crbug.com/385172647 - Figure out why secondary text doesn't have start padding in
        // the RTL layout.
        ChipView singleLineChip =
                new ChipView(mActivityTestRule.getActivity(), null, 0, R.style.AssistiveChip);
        ChipView twoLineChip =
                (ChipView)
                        mActivityTestRule
                                .getActivity()
                                .getLayoutInflater()
                                .inflate(R.layout.two_line_chip_view_test_item, null);

        List<ChipView> chips = List.of(singleLineChip, twoLineChip);

        for (ChipView chip : chips) {
            chip.getPrimaryTextView().setText("Primary text");
            chip.getSecondaryTextView().setText("Secondary text");
            chip.setIcon(R.drawable.ic_settings_gear_24dp, /* tintWithTextColor= */ true);
            chip.addRemoveIcon();
            runOnUiThreadBlocking(
                    () -> {
                        mContentView.addView(
                                chip,
                                new LayoutParams(
                                        LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
                    });
        }

        mRenderTestRule.render(mContentView, "chip_views");
    }
}
