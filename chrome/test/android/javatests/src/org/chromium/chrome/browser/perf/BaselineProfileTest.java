// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.perf;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import androidx.benchmark.macro.junit4.BaselineProfileRule;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import kotlin.Unit;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.IntegrationTest;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.net.test.EmbeddedTestServerRule;

/**
 * This test creates a baseline profile for chrome. See
 * https://developer.android.com/topic/performance/baselineprofiles/overview
 */
@DoNotBatch(reason = "This test communicates with the runner using Instrumentation Statuses.")
@RunWith(AndroidJUnit4.class)
public class BaselineProfileTest {
    private static final String TAG = "BaselineProfileTest";
    private static final String ACTIVITY_NAME = "com.google.android.apps.chrome.IntentDispatcher";
    private static final String TEST_PAGE =
            "/chrome/test/android/javatests/src/org/chromium/chrome/browser/perf/test.html";
    private static final String CCT_SESSION_EXTRA = "android.support.customtabs.extra.SESSION";
    private static final String PACKAGE_NAME_ARG =
            "org.chromium.chrome.test.pagecontroller.rules."
                    + "ChromeUiApplicationTestRule.PackageUnderTest";

    @ClassRule
    public static EmbeddedTestServerRule sEmbeddedTestServerRule = new EmbeddedTestServerRule();

    @Rule public BaselineProfileRule mBaselineProfileRule = new BaselineProfileRule();
    private String mPackageName;

    @Before
    public void setUp() {
        mPackageName =
                InstrumentationRegistry.getArguments()
                        .getString(PACKAGE_NAME_ARG, "org.chromium.chrome");
    }

    @Test
    @IntegrationTest
    public void testGenerateBaselineProfile() {
        Context context = ApplicationProvider.getApplicationContext();
        String url = sEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);
        mBaselineProfileRule.collect(
                /* packageName= */ mPackageName,
                /* maxIterations= */ 10,
                /* stableIterations= */ 3,
                /* outputFilePrefix= */ null,
                /* includeInStartupProfile= */ true,
                /* profileBlock= */ scope -> {
                    final Intent cct_intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
                    cct_intent.addFlags(
                            Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);
                    // Mark it as a CCT session.
                    cct_intent.putExtra(CCT_SESSION_EXTRA, (Bundle) null);
                    cct_intent.setComponent(new ComponentName(mPackageName, ACTIVITY_NAME));
                    Log.i(TAG, "startActivity(CCT)");
                    context.startActivity(cct_intent);
                    IUi2Locator locatorChrome = Ui2Locators.withPackageName(mPackageName);
                    // Chrome starts this block dead, wait for it to load.
                    Log.i(TAG, "Waiting for chrome to load");
                    UiAutomatorUtils.getInstance().waitUntilAnyVisible(locatorChrome);
                    Log.i(TAG, "Waiting for Top Bar to show Host");
                    String origin = sEmbeddedTestServerRule.getOrigin();
                    assert origin.startsWith("http://");
                    String host = origin.substring(7, origin.length() - 1);
                    IUi2Locator hostTextLocator = Ui2Locators.withText(host);
                    UiAutomatorUtils.getInstance().waitUntilAnyVisible(hostTextLocator);
                    Log.i(TAG, "CCT load complete");

                    final Intent cta_intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
                    cta_intent.addCategory(Intent.CATEGORY_BROWSABLE);
                    cta_intent.addCategory(Intent.CATEGORY_DEFAULT);
                    cta_intent.addFlags(
                            Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);
                    cta_intent.setComponent(new ComponentName(mPackageName, ACTIVITY_NAME));
                    Log.i(TAG, "startActivity(CTA)");
                    context.startActivity(cta_intent);
                    String urlWithoutScheme = url.substring(7);
                    IUi2Locator urlTextLocator = Ui2Locators.withText(urlWithoutScheme);
                    Log.i(TAG, "Waiting for omnibox to show URL");
                    UiAutomatorUtils.getInstance().waitUntilAnyVisible(urlTextLocator);
                    Log.i(TAG, "CTA load complete");

                    return Unit.INSTANCE;
                });
    }
}
