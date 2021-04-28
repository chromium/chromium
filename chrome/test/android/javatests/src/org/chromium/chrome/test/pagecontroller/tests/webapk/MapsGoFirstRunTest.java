// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.tests.webapk;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.firstrun.FirstRunActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.firstrun.LightweightFirstRunActivity;
import org.chromium.chrome.browser.webapps.WebappActivity;
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
 * Test the Maps Go First Run Experience. This test will be mostly focus on verifying the
 * lightweight first run is activated in different scenarios.
 */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class MapsGoFirstRunTest {
    private static final String TAG = "MapsGoFirstRunTest";
    private static final String FLAG_POLICY_TOS_DIALOG_BEHAVIOR_STANDARD =
            "policy={\"TosDialogBehavior\":1}";
    private static final String FLAG_POLICY_TOS_DIALOG_BEHAVIOR_SKIP =
            "policy={\"TosDialogBehavior\":2}";

    // Launching the PWA is currently taking 9-10 seconds on emulators. Increasing this
    // substantially to avoid flakes. See https://crbug.com/1142821.
    private static final long MAPS_GO_FRE_TIMEOUT_MS = 20000L;

    public ChromeUiAutomatorTestRule mUiAutomatorRule = new ChromeUiAutomatorTestRule();
    public ChromeUiApplicationTestRule mChromeUiRule = new ChromeUiApplicationTestRule();

    @Rule
    public final TestRule mChain = RuleChain.outerRule(mChromeUiRule).around(mUiAutomatorRule);

    private Activity mLightweightFreActivity;
    private Activity mFirstRunActivity;
    private Activity mWebappActivity;
    private ApplicationStatus.ActivityStateListener mActivityStateListener;
    private final CallbackHelper mFreStoppedCallback = new CallbackHelper();

    @Before
    public void setUp() {
        WebApkValidator.setDisableValidationForTesting(true);
        TestThreadUtils.runOnUiThreadBlocking(WebappRegistry::refreshSharedPrefsForTesting);

        mActivityStateListener = (activity, newState) -> {
            if (activity instanceof LightweightFirstRunActivity) {
                if (mLightweightFreActivity == null) mLightweightFreActivity = activity;
                if (newState == ActivityState.STOPPED) mFreStoppedCallback.notifyCalled();
            } else if (activity instanceof FirstRunActivity) {
                if (mFirstRunActivity == null) mFirstRunActivity = activity;
            } else if (activity instanceof WebappActivity) {
                if (mWebappActivity == null) mWebappActivity = activity;
            }
        };
        ApplicationStatus.registerStateListenerForAllActivities(mActivityStateListener);
    }

    @After
    public void tearDown() {
        LightweightFirstRunActivity.setSupportSkippingTos(true);
        ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
    }

    @Test
    public void testFirstRunIsShown() {
        LightweightFirstRunActivity.setSupportSkippingTos(false);
        FirstRunStatus.setLightweightFirstRunFlowComplete(false);
        launchWebapk("org.chromium.test.maps_go_webapk", "org.chromium.chrome");

        LightWeightTOSController controller = LightWeightTOSController.getInstance();
        Assert.assertTrue("Light weight TOS page should be shown.", controller.isCurrentPageThis());

        controller.acceptAndContinue();
        // Note for offline devices this PWA will not be healthy, see https://crbug.com/1142821 for
        // details. Just verify the right activity has started.
        CriteriaHelper.pollInstrumentationThread(
                () -> mWebappActivity != null, "WebappActivity did not start.");
    }

    @Test
    public void testFirstRunFallbackForInvalidPwa() {
        // Verification will fail for this APK, so instead of using the LWFRE, the full FRE will be
        // shown instead.
        WebApkValidator.setDisableValidationForTesting(false);
        launchWebapk("org.chromium.test.maps_go_webapk", "org.chromium.chrome");
        CriteriaHelper.pollInstrumentationThread(
                () -> mFirstRunActivity != null, "FirstRunActivity did not start");
        Assert.assertNull("Lightweight FRE should not have started.", mLightweightFreActivity);
    }

    @Test
    public void testFirstRunIsNotShownAfterAck() {
        LightweightFirstRunActivity.setSupportSkippingTos(false);
        FirstRunStatus.setLightweightFirstRunFlowComplete(true);
        launchWebapk("org.chromium.test.maps_go_webapk", "org.chromium.chrome");

        LightWeightTOSController controller = LightWeightTOSController.getInstance();
        Assert.assertFalse(
                "Light weight TOS page should NOT be shown.", controller.isCurrentPageThis());
        Assert.assertNull("Lightweight FRE should not launch.", mLightweightFreActivity);
    }

    @Test
    @CommandLineFlags.Add({"force-device-ownership", FLAG_POLICY_TOS_DIALOG_BEHAVIOR_SKIP})
    public void testTosSkipped() throws Exception {
        LightweightFirstRunActivity.setSupportSkippingTos(true);
        FirstRunStatus.setLightweightFirstRunFlowComplete(false);
        launchWebapk("org.chromium.test.maps_go_webapk", "org.chromium.chrome");

        Assert.assertNotNull("Lightweight FRE should launch.", mLightweightFreActivity);

        LightWeightTOSController controller = LightWeightTOSController.getInstance();
        Assert.assertFalse(
                "Light weight TOS page should NOT be shown.", controller.isCurrentPageThis());
        mFreStoppedCallback.waitForCallback("Lightweight Fre never completes.", 0);
    }

    @Test
    @CommandLineFlags.Add({FLAG_POLICY_TOS_DIALOG_BEHAVIOR_STANDARD})
    public void testTosNotSkippedByPolicy() {
        LightweightFirstRunActivity.setSupportSkippingTos(true);
        FirstRunStatus.setLightweightFirstRunFlowComplete(false);
        launchWebapk("org.chromium.test.maps_go_webapk", "org.chromium.chrome");

        Assert.assertNotNull("Lightweight FRE should launch.", mLightweightFreActivity);
        LightWeightTOSController controller = LightWeightTOSController.getInstance();
        Assert.assertTrue("Light weight TOS page should be shown.", controller.isCurrentPageThis());
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
}
