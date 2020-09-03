// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.app.Activity;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.BuildInfo;
import org.chromium.base.FeatureList;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.PromoDialog;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

import java.util.Collections;

/**
 * Instrument test for {@link DefaultBrowserPromoManager}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DefaultBrowserPromoManagerTest extends DummyUiActivityTestCase {
    private DefaultBrowserPromoManager mManager;
    private Activity mActivity;
    private String mAppName;
    private WindowAndroid mWindowAndroid;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        mActivity = getActivity();
        mWindowAndroid = TestThreadUtils.runOnUiThreadBlocking(() -> new WindowAndroid(mActivity));
        mManager = new DefaultBrowserPromoManager(mActivity, new ActivityLifecycleDispatcher() {
            @Override
            public void register(LifecycleObserver observer) {}

            @Override
            public void unregister(LifecycleObserver observer) {}

            @Override
            public int getCurrentActivityState() {
                return 0;
            }

            @Override
            public boolean isNativeInitializationFinished() {
                return false;
            }
        }, mWindowAndroid, DefaultBrowserState.NO_DEFAULT);
        mAppName = BuildInfo.getInstance().hostPackageLabel;
        // Enabling feature can assign a default value to the fieldtrial param.
        FeatureList.setTestFeatures(Collections.EMPTY_MAP);
    }

    @Override
    public void tearDownTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(mWindowAndroid::destroy);
        super.tearDownTest();
    }

    @Test
    @MediumTest
    public void testPromoByRoleManager() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mManager.promoByRoleManager(); });
        DefaultBrowserPromoDialog dialog = mManager.getDialogForTesting();
        Assert.assertEquals("Dialog should be of role manager style on Q+",
                dialog.getDialogStyleForTesting(),
                DefaultBrowserPromoDialog.DialogStyle.ROLE_MANAGER);

        // test role manager style
        PromoDialog.DialogParams params = dialog.getDialogParams();
        Assert.assertEquals(
                mActivity.getString(R.string.default_browser_promo_dialog_title, mAppName),
                params.headerCharSequence);

        Assert.assertEquals(
                mActivity.getString(R.string.default_browser_promo_dialog_desc, mAppName) + "\n\n"
                        + mActivity.getString(
                                R.string.default_browser_promo_dialog_role_manager_steps, mAppName),
                params.subheaderCharSequence);

        Assert.assertEquals(
                mActivity.getString(
                        R.string.default_browser_promo_dialog_choose_chrome_button, mAppName),
                params.primaryButtonCharSequence);

        checkDialogVisibility();
    }

    @Test
    @MediumTest
    public void testPromoBySystemSettingsOnP() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mManager.promoBySystemSettings(); });
        DefaultBrowserPromoDialog dialog = mManager.getDialogForTesting();
        Assert.assertEquals(
                "Dialog should be of system settings style on P-, when there is another default in system",
                dialog.getDialogStyleForTesting(),
                DefaultBrowserPromoDialog.DialogStyle.SYSTEM_SETTINGS);

        // test role manager style
        PromoDialog.DialogParams params = dialog.getDialogParams();
        Assert.assertEquals(
                mActivity.getString(R.string.default_browser_promo_dialog_title, mAppName),
                params.headerCharSequence);

        Assert.assertEquals(
                mActivity.getString(R.string.default_browser_promo_dialog_desc, mAppName) + "\n\n"
                        + mActivity.getString(
                                R.string.default_browser_promo_dialog_system_settings_steps,
                                mAppName),
                params.subheaderCharSequence);

        Assert.assertEquals(
                mActivity.getString(R.string.default_browser_promo_dialog_go_to_settings_button),
                params.primaryButtonCharSequence);

        checkDialogVisibility();
    }

    @Test
    @MediumTest
    public void testPromoByDisambiguationSheet() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mManager.promoByDisambiguationSheet(); });
        DefaultBrowserPromoDialog dialog = mManager.getDialogForTesting();
        Assert.assertEquals(
                "Dialog should be of disambiguation sheet style on P-, when there is no default in system",
                dialog.getDialogStyleForTesting(),
                DefaultBrowserPromoDialog.DialogStyle.DISAMBIGUATION_SHEET);

        // test role manager style
        PromoDialog.DialogParams params = dialog.getDialogParams();
        Assert.assertEquals(
                mActivity.getString(R.string.default_browser_promo_dialog_title, mAppName),
                params.headerCharSequence);

        Assert.assertEquals(
                mActivity.getString(R.string.default_browser_promo_dialog_desc, mAppName) + "\n\n"
                        + mActivity.getString(
                                R.string.default_browser_promo_dialog_disambiguation_sheet_steps,
                                mAppName),
                params.subheaderCharSequence);

        Assert.assertEquals(
                mActivity.getString(
                        R.string.default_browser_promo_dialog_choose_chrome_button, mAppName),
                params.primaryButtonCharSequence);

        checkDialogVisibility();
    }

    private void checkDialogVisibility() {
        onView(withId(R.id.promo_dialog_layout)).check(matches(isDisplayed()));
        // dismiss the dialog
        onView(withId(R.id.button_secondary)).perform(click());
        onView(withId(R.id.promo_dialog_layout)).check((v, noMatchingViewException) -> {
            Assert.assertNotNull("Promo dialog should be dismissed by clicking on secondary button",
                    noMatchingViewException);
        });
    }
}
