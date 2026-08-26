// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.search;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.app.Activity;
import android.graphics.Color;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.util.Arrays;
import java.util.List;

/** These tests render screenshots of the SearchBoxView and compare them to a gold standard. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class SearchBoxViewRenderTest {

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false, false).name("Default"),
                    new ParameterSet().value(true, false).name("NightMode"));

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE)
                    .setRevision(1)
                    .build();

    private final boolean mNightModeEnabled;
    private ViewGroup mContentView;

    public SearchBoxViewRenderTest(boolean nightModeEnabled, boolean useRtlLayout) {
        mNightModeEnabled = nightModeEnabled;
        setRtlForTesting(useRtlLayout);
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(useRtlLayout ? "RTL" : "LTR");
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(/* startIntent= */ null);
        Activity activity = mActivityTestRule.getActivity();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContentView =
                runOnUiThreadBlocking(
                        () -> {
                            LinearLayout contentView = new LinearLayout(activity);
                            contentView.setOrientation(LinearLayout.VERTICAL);
                            contentView.setBackgroundColor(
                                    mNightModeEnabled ? Color.BLACK : Color.WHITE);

                            activity.setContentView(
                                    contentView,
                                    new LayoutParams(
                                            LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
                            return contentView;
                        });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void renderDefaultMobileMode() throws Exception {
        SearchBoxView searchBox =
                runOnUiThreadBlocking(
                        () -> {
                            SearchBoxView view =
                                    new SearchBoxView(mActivityTestRule.getActivity(), null);
                            view.setHintText("Search your stuff");
                            return view;
                        });

        renderSearchBox(searchBox, "search_box_view_default");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void renderDesktopMode() throws Exception {
        SearchBoxView searchBox =
                runOnUiThreadBlocking(
                        () -> {
                            SearchBoxView view =
                                    new SearchBoxView(mActivityTestRule.getActivity(), null);
                            view.setHintText("Search your stuff");
                            view.setDesktopMode(true);
                            return view;
                        });

        renderSearchBox(searchBox, "search_box_view_desktop");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void renderWithTextAndClearButtonVisible() throws Exception {
        SearchBoxView searchBox =
                runOnUiThreadBlocking(
                        () -> {
                            SearchBoxView view =
                                    new SearchBoxView(mActivityTestRule.getActivity(), null);
                            view.setHintText("Search your stuff");
                            view.setSearchText("hello world");
                            view.setClearButtonVisibility(true);
                            view.setDesktopMode(true);
                            return view;
                        });

        renderSearchBox(searchBox, "search_box_view_with_text");
    }

    private void renderSearchBox(SearchBoxView searchBoxView, String name) throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    mContentView.removeAllViews();
                    mContentView.addView(
                            searchBoxView,
                            new LinearLayout.LayoutParams(
                                    LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
                });

        mRenderTestRule.render(mContentView, name);
    }
}
