// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import android.content.ComponentName;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.util.ChromePackageNameVariant;
import org.chromium.chrome.browser.util.DefaultBrowserInfo.DefaultBrowserState;

/** Unit test for {@link DefaultBrowserStateProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DefaultBrowserStateProviderTest {
    DefaultBrowserStateProvider mProvider;

    @Before
    public void setUp() {
        mProvider = new DefaultBrowserStateProvider();
    }

    @Test
    public void testGetCurrentDefaultStateForNoDefault() {
        Assert.assertEquals(
                "Should be no default when resolve info matches no browser.",
                DefaultBrowserState.NO_DEFAULT,
                mProvider.getCurrentDefaultBrowserState(createResolveInfo("android", 0), false));
    }

    @Test
    public void testGetCurrentDefaultStateForOtherDefault() {
        Assert.assertEquals(
                "Should be other default when resolve info matches another browser.",
                DefaultBrowserState.OTHER_DEFAULT,
                mProvider.getCurrentDefaultBrowserState(createResolveInfo("android", 1), false));
    }

    @Test
    public void testGetCurrentDefaultStateForChromeDefault() {
        Assert.assertEquals(
                "Should be chrome default when resolve info matches current package name.",
                DefaultBrowserState.CHROME_DEFAULT,
                mProvider.getCurrentDefaultBrowserState(
                        createResolveInfo(ContextUtils.getApplicationContext().getPackageName(), 1),
                        false));
    }

    @Test
    public void testIsChromePreStableInstalled() {
        ShadowPackageManager packageManager =
                Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager());
        // Setting android_manifest in the junit_binary build rule causes the current package to
        // appear in the PackageManager.
        packageManager.deletePackage(RuntimeEnvironment.getApplication().getPackageName());

        addBrowser(packageManager, ChromePackageNameVariant.CHROME_STABLE_PACKAGE_NAME);
        Assert.assertFalse(
                "Chrome stable should not be counted as a pre-stable channel",
                mProvider.isChromePreStableInstalled());

        addBrowser(packageManager, "com.android.chrome.123");
        Assert.assertFalse(
                "A random package should not be counted as a pre-stable channel",
                mProvider.isChromePreStableInstalled());

        for (String name : ChromePackageNameVariant.CHROME_PACKAGE_NAMES) {
            if (name.equals(ChromePackageNameVariant.CHROME_STABLE_PACKAGE_NAME)) continue;
            ComponentName browser = addBrowser(packageManager, name);
            Assert.assertTrue(
                    name + " should be considered as a pre-stable channel",
                    mProvider.isChromePreStableInstalled());
            // Remove it again so that each channel is checked on its own.
            packageManager.removeActivity(browser);
        }
    }

    /** Adds an activity that handles {@link PackageManagerUtils#BROWSER_INTENT}. */
    private static ComponentName addBrowser(
            ShadowPackageManager packageManager, String packageName) {
        ComponentName browser = new ComponentName(packageName, packageName + ".BrowserActivity");
        packageManager.addActivityIfNotPresent(browser);
        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        // PackageManagerUtils#queryAllWebBrowsersInfo() passes MATCH_DEFAULT_ONLY.
        filter.addCategory(Intent.CATEGORY_DEFAULT);
        filter.addDataScheme("http");
        packageManager.addIntentFilterForActivity(browser, filter);
        return browser;
    }

    private ResolveInfo createResolveInfo(String packageName, int match) {
        ResolveInfo resolveInfo = new ResolveInfo();
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = packageName;
        resolveInfo.activityInfo = activityInfo;
        resolveInfo.match = match;
        return resolveInfo;
    }
}
