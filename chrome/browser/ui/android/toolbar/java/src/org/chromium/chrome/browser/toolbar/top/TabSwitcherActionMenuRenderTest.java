// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.List;

/**
 * Render tests for tab switcher long-press menu popup.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class TabSwitcherActionMenuRenderTest extends DummyUiActivityTestCase {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    private View mView;

    public TabSwitcherActionMenuRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForDummyUiActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Activity activity = getActivity();
            TabSwitcherActionMenuCoordinator coordinator = new TabSwitcherActionMenuCoordinator();

            coordinator.displayMenu(activity, new ListMenuButton(activity, null),
                    coordinator.buildMenuItems(), null);

            mView = coordinator.getContentView();
            ((ViewGroup) mView.getParent()).removeView(mView);

            int popupWidth =
                    activity.getResources().getDimensionPixelSize(R.dimen.tab_switcher_menu_width);
            mView.setBackground(ApiCompatibilityUtils.getDrawable(
                    activity.getResources(), R.drawable.popup_bg_tinted));
            activity.setContentView(mView, new LayoutParams(popupWidth, WRAP_CONTENT));
        });
    }

    @Override
    public void tearDownTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { NightModeTestUtils.tearDownNightModeForDummyUiActivity(); });
        super.tearDownTest();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_TabSwitcherActionMenu() throws IOException {
        mRenderTestRule.render(mView, "tab_switcher_action_menu");
    }
}
