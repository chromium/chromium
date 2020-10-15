// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.tests.webapk;

import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.test.pagecontroller.controllers.webapk.first_run.LightWeightTOSController;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiApplicationTestRule;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiAutomatorTestRule;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.chrome.test.pagecontroller.utils.UiLocatorHelper;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Test the Maps Go First Run Experience.
 */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class MapsGoFirstRunTest {
    private static final String TAG = "MapsGoFirstRunTest";
    private static final long MAPS_GO_FRE_TIMEOUT_MS = 9000L;
    public ChromeUiAutomatorTestRule mUiAutomatorRule = new ChromeUiAutomatorTestRule();
    public ChromeUiApplicationTestRule mChromeUiRule = new ChromeUiApplicationTestRule();

    @Rule
    public final TestRule mChain = RuleChain.outerRule(mChromeUiRule).around(mUiAutomatorRule);

    @Before
    public void setUp() {
        WebApkValidator.setDisableValidationForTesting(true);
        TestThreadUtils.runOnUiThreadBlocking(WebappRegistry::refreshSharedPrefsForTesting);
    }

    @Test
    public void testFirstRunIsShown() {
        FirstRunStatus.setLightweightFirstRunFlowComplete(false);
        launchWebapk("org.chromium.test.maps_go_webapk", "org.chromium.chrome");

        LightWeightTOSController controller = LightWeightTOSController.getInstance();
        Assert.assertTrue("Light weight TOS page should be shown.", controller.isCurrentPageThis());

        controller.acceptAndContinue();
        verifyRunningInChromeBannerOnScreen();
    }

    @Test
    public void testFirstRunIsNotShownAfterAck() {
        FirstRunStatus.setLightweightFirstRunFlowComplete(true);
        launchWebapk("org.chromium.test.maps_go_webapk", "org.chromium.chrome");

        LightWeightTOSController controller = LightWeightTOSController.getInstance();
        Assert.assertFalse(
                "Light weight TOS page should NOT be shown.", controller.isCurrentPageThis());
    }

    /**
     * Launch a WebAPK (which launches Chrome).
     * @param webapkPackageName Package name of the WebAPK.
     * @param chromePackageName Package name of Chromium that the WebAPK points to.
     */
    private void launchWebapk(String webapkPackageName, String chromePackageName) {
        Log.d(TAG, "Launching %s in Chrome (%s)", webapkPackageName, chromePackageName);

        Context context = InstrumentationRegistry.getContext();
        final Intent intent =
                context.getPackageManager().getLaunchIntentForPackage(webapkPackageName);
        if (intent == null) {
            throw new IllegalStateException("Could not get intent to launch " + webapkPackageName
                    + ", please ensure that it is installed");
        }
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);

        UiLocatorHelper helper =
                UiAutomatorUtils.getInstance().getLocatorHelper(MAPS_GO_FRE_TIMEOUT_MS);
        IUi2Locator packageLocator = Ui2Locators.withPackageName(chromePackageName);
        helper.verifyOnScreen(packageLocator);
    }

    private void verifyRunningInChromeBannerOnScreen() {
        UiLocatorHelper helper =
                UiAutomatorUtils.getInstance().getLocatorHelper(MAPS_GO_FRE_TIMEOUT_MS);
        IUi2Locator runningInChromeBanner =
                Ui2Locators.withContentDescString(R.string.twa_running_in_chrome);
        helper.verifyOnScreen(runningInChromeBanner);
    }
}
