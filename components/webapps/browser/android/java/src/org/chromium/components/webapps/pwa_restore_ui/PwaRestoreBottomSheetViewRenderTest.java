// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.app.Activity;
import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import androidx.test.filters.SmallTest;

import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.List;

/** Render test for {@link PwaRestoreBottomSheetView}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@Batch(Batch.UNIT_TESTS)
public class PwaRestoreBottomSheetViewRenderTest {
    private static Activity sActivity;

    @ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setCorpus(RenderTestRule.Corpus.ANDROID_RENDER_TESTS_PUBLIC)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_WEB_APP_INSTALLS)
                    .build();

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity = sActivityTestRule.getActivity();
                });
    }

    public PwaRestoreBottomSheetViewRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mNightModeEnabled = nightModeEnabled;
    }

    private PwaRestoreBottomSheetCoordinator mCoordinator;
    private PropertyModel mModel;

    private final boolean mNightModeEnabled;

    private void initializeBottomSheet() {
        mCoordinator =
                new PwaRestoreBottomSheetCoordinator(
                        sActivity, null, R.drawable.ic_arrow_back_24dp);
        PropertyModel model = mCoordinator.getModelForTesting();
        model.set(PwaRestoreProperties.VIEW_STATE, PwaRestoreProperties.ViewState.PREVIEW);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LinearLayout content = new LinearLayout(sActivity);
                    sActivity.setContentView(content);
                    View view = mCoordinator.getBottomSheetToolbarViewForTesting();
                    View root = view.getRootView();
                    root.setBackgroundColor(mNightModeEnabled ? Color.BLACK : Color.WHITE);

                    content.addView(
                            view,
                            new LayoutParams(
                                    ViewGroup.LayoutParams.WRAP_CONTENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT));
                });
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testPeeking() throws Exception {
        initializeBottomSheet();
        mRenderTestRule.render(
                mCoordinator.getBottomSheetToolbarViewForTesting(), "pwa_restore_peeking");
    }
}
