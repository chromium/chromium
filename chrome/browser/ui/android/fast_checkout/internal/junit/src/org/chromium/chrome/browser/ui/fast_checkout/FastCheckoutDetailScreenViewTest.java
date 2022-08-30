// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_BACK_CLICK_HANDLER;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DETAIL_SCREEN_SETTINGS_CLICK_HANDLER;

import android.view.LayoutInflater;
import android.view.View;

import androidx.appcompat.widget.Toolbar;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.fast_checkout.detail_screen.DetailScreenCoordinator;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Simple unit tests for the detail screen view.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.DISABLE_NATIVE_INITIALIZATION})
public class FastCheckoutDetailScreenViewTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public TestRule mCommandLineFlagsRule = CommandLineFlags.getTestRule();
    @Rule
    public ActivityScenarioRule<ChromeTabbedActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(ChromeTabbedActivity.class);

    @Mock
    private Runnable mBackClickHandler;
    @Mock
    private Runnable mSettingsClickHandler;

    private PropertyModel mModel;
    private View mView;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> {
            mModel = FastCheckoutProperties.createDefaultModel();
            mModel.set(DETAIL_SCREEN_SETTINGS_CLICK_HANDLER,
                    FastCheckoutMediator.createSettingsOnClickListener(mSettingsClickHandler));
            mModel.set(DETAIL_SCREEN_BACK_CLICK_HANDLER, mBackClickHandler);

            // Create the view.
            mView = LayoutInflater.from(activity).inflate(
                    R.layout.fast_checkout_detail_screen_sheet, null);

            // Let the coordinator connect model and view.
            new DetailScreenCoordinator(activity, mView, mModel);
        });
    }

    public static <T> T waitForEvent(T mock) {
        return verify(mock,
                timeout(ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }

    @Test
    @SmallTest
    public void testBackArrowClickCallsHandler() {
        assertNotNull(mView);
        Toolbar toolbar = mView.findViewById(R.id.action_bar);
        assertNotNull(toolbar);

        // Find the navigation button. Toolbar does not expose a method to get
        // the navigation button and Espresso does not work in this setup.
        // TODO(crbug.com/1355310): Move to integration test once that exists.
        View backButton = null;
        for (int index = 0; index < toolbar.getChildCount(); ++index) {
            View candidateView = toolbar.getChildAt(index);
            if (candidateView.getContentDescription() != null
                    && candidateView.getContentDescription().equals(
                            toolbar.getNavigationContentDescription())) {
                backButton = candidateView;
            }
        }
        assertNotNull(backButton);
        backButton.performClick();
        waitForEvent(mBackClickHandler).run();
    }

    @Test
    @SmallTest
    public void testOpenSettingsClickCallsHandler() {
        assertNotNull(mView);

        // Click on the settings element.
        View settingsMenuElement = mView.findViewById(R.id.settings_menu_id);
        assertNotNull(settingsMenuElement);
        settingsMenuElement.performClick();
        waitForEvent(mSettingsClickHandler).run();
    }
}
