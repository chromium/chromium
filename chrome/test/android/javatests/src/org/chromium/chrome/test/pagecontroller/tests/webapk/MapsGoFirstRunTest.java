// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.tests.webapk;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.test.InstrumentationRegistry;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.firstrun.FirstRunActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
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
    private static final String MAPS_GO_PACKAGE = "org.chromium.test.maps_go_webapk";

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

    @Before
    public void setUp() {
        WebApkValidator.setDisableValidationForTesting(true);
        FirstRunUtils.setDisableDelayOnExitFreForTest(true);
        TestThreadUtils.runOnUiThreadBlocking(WebappRegistry::refreshSharedPrefsForTesting);

        mActivityStateListener = (activity, newState) -> {
            if (activity instanceof LightweightFirstRunActivity) {
                if (mLightweightFreActivity == null) mLightweightFreActivity = activity;
            } else if (activity instanceof FirstRunActivity) {
                if (mFirstRunActivity == null) mFirstRunActivity = activity;
            } else if (activity instanceof WebappActivity) {
                if (mWebappActivity == null) mWebappActivity = activity;
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ApplicationStatus.registerStateListenerForAllActivities(mActivityStateListener);
        });
    }

    @After
    public void tearDown() {
        WebApkValidator.setDisableValidationForTesting(false);
        FirstRunUtils.setDisableDelayOnExitFreForTest(false);
        LightweightFirstRunActivity.setSupportSkippingTos(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
        });
    }

    @Test
    public void testFirstRunIsShown() {
        LightweightFirstRunActivity.setSupportSkippingTos(false);
        FirstRunStatus.setLightweightFirstRunFlowComplete(false);
        launchWebapk();

        LightWeightTOSController controller = LightWeightTOSController.getInstance();
        Assert.assertTrue("Light weight TOS page should be shown.", controller.isCurrentPageThis());

        controller.acceptAndContinue();
        // Note for offline devices this PWA will not be healthy, see https://crbug.com/1142821 for
        // details. Just verify the right activity has started.
        verifyWebappActivityStarted();
    }

    @Test
    public void testFirstRunFallbackForInvalidPwa() {
        // Verification will fail for this APK, so instead of using the LWFRE, the full FRE will be
        // shown instead.
        WebApkValidator.setDisableValidationForTesting(false);
        launchWebapk();

        CriteriaHelper.pollInstrumentationThread(
                () -> mFirstRunActivity != null, "FirstRunActivity did not start");
        Assert.assertNull("Lightweight FRE should not have started.", mLightweightFreActivity);
    }

    @Test
    public void testFirstRunIsNotShownAfterAck() {
        LightweightFirstRunActivity.setSupportSkippingTos(false);
        FirstRunStatus.setLightweightFirstRunFlowComplete(true);
        launchWebapk();

        verifyWebappActivityStarted();
        Assert.assertNull("Lightweight FRE should not have started.", mLightweightFreActivity);
    }

    @Test
    @CommandLineFlags.Add({"force-device-ownership", FLAG_POLICY_TOS_DIALOG_BEHAVIOR_SKIP})
    public void testTosSkipped() throws Exception {
        LightweightFirstRunActivity.setSupportSkippingTos(true);
        FirstRunStatus.setLightweightFirstRunFlowComplete(false);
        launchWebapk();

        // Verify LWFRE activity is created before skipped to WebappActivity. See
        // https://crbug.com/1184149 for previous problems here.
        CriteriaHelper.pollInstrumentationThread(()
                                                         -> mLightweightFreActivity != null,
                "Lightweight FRE should still launch before being skipped.");
        verifyWebappActivityStarted();
    }

    @Test
    @CommandLineFlags.Add({FLAG_POLICY_TOS_DIALOG_BEHAVIOR_STANDARD})
    public void testTosNotSkippedByPolicy() {
        LightweightFirstRunActivity.setSupportSkippingTos(true);
        FirstRunStatus.setLightweightFirstRunFlowComplete(false);
        launchWebapk();

        Assert.assertNotNull("Lightweight FRE should launch.", mLightweightFreActivity);
        LightWeightTOSController controller = LightWeightTOSController.getInstance();
        Assert.assertTrue("Light weight TOS page should be shown.", controller.isCurrentPageThis());
    }

    /**
     * Launch the Maps Go WebAPK.
     *
     * Maps Go WebAPK is a bound WebAPK to the apk under test so that there is
     * no browser choice on launch.
     */
    private void launchWebapk() {
        String chromePackageName = InstrumentationRegistry.getTargetContext().getPackageName();
        Log.d(TAG, "Launching %s in Chrome (%s)", MAPS_GO_PACKAGE, chromePackageName);

        Context context = ApplicationProvider.getApplicationContext();
        final Intent intent =
                context.getPackageManager().getLaunchIntentForPackage(MAPS_GO_PACKAGE);
        if (intent == null) {
            throw new IllegalStateException("Could not get intent to launch " + MAPS_GO_PACKAGE
                    + ", please ensure that it is installed");
        }
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);

        UiLocatorHelper helper =
                UiAutomatorUtils.getInstance().getLocatorHelper(MAPS_GO_FRE_TIMEOUT_MS);
        IUi2Locator packageLocator = Ui2Locators.withPackageName(chromePackageName);
        helper.verifyOnScreen(packageLocator);
    }

    private void verifyWebappActivityStarted() {
        CriteriaHelper.pollInstrumentationThread(()
                                                         -> mWebappActivity != null,
                "WebappActivity did not start.", MAPS_GO_FRE_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}
