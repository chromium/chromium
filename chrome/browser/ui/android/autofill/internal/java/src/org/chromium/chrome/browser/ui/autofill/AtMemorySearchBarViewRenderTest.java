// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.app.Activity;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.ViewUtils;

import java.util.List;

/** Render tests for {@link AtMemorySearchBarView}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@LargeTest
@DoNotBatch(reason = "Night mode testing requires fresh activity")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AtMemorySearchBarViewRenderTest {
    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setDescription("Strings updated")
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_AUTOFILL)
                    .build();

    private Activity mActivity;
    private AtMemorySearchBarView mView;

    public AtMemorySearchBarViewRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        sActivityTestRule.launchActivity(null);
        mActivity = sActivityTestRule.getActivity();
    }

    @After
    public void tearDown() {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    @Test
    @Feature({"RenderTest"})
    public void testNormalState() throws Exception {
        ContextThemeWrapper themeWrapper =
                new ContextThemeWrapper(mActivity, R.style.Theme_BrowserUI_DayNight);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewGroup content = mActivity.findViewById(android.R.id.content);
                    content.removeAllViews();
                    mView =
                            (AtMemorySearchBarView)
                                    LayoutInflater.from(themeWrapper)
                                            .inflate(R.layout.at_memory_search_bar, content, false);
                    content.addView(mView);
                });

        ViewUtils.waitForStableView(mView);
        mRenderTestRule.render(mView, "at_memory_search_bar_normal");
    }

    @Test
    @Feature({"RenderTest"})
    public void testLoadingState() throws Exception {
        ContextThemeWrapper themeWrapper =
                new ContextThemeWrapper(mActivity, R.style.Theme_BrowserUI_DayNight);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewGroup content = mActivity.findViewById(android.R.id.content);
                    content.removeAllViews();
                    mView =
                            (AtMemorySearchBarView)
                                    LayoutInflater.from(themeWrapper)
                                            .inflate(R.layout.at_memory_search_bar, content, false);
                    mView.setIsLoading(true);
                    content.addView(mView);
                });

        ViewUtils.waitForStableView(mView);
        mRenderTestRule.render(mView, "at_memory_search_bar_loading");
    }
}
