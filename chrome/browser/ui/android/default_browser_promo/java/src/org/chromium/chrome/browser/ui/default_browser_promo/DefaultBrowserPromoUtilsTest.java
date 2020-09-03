// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserState;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit test for {@link DefaultBrowserPromoUtils}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DefaultBrowserPromoUtilsTest {
    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testGetCurrentDefaultStateForNoDefault() {
        Assert.assertEquals("Should be no default when resolve info matches no browser.",
                DefaultBrowserState.NO_DEFAULT,
                DefaultBrowserPromoDeps.getInstance().getCurrentDefaultBrowserState(
                        createResolveInfo("android", 0)));
    }

    @Test
    public void testGetCurrentDefaultStateForOtherDefault() {
        Assert.assertEquals("Should be other default when resolve info matches another browser.",
                DefaultBrowserPromoUtils.DefaultBrowserState.OTHER_DEFAULT,
                DefaultBrowserPromoDeps.getInstance().getCurrentDefaultBrowserState(
                        createResolveInfo("android", 1)));
    }

    @Test
    public void testGetCurrentDefaultStateForChromeDefault() {
        Assert.assertEquals(
                "Should be chrome default when resolve info matches current package name.",
                DefaultBrowserPromoUtils.DefaultBrowserState.CHROME_DEFAULT,
                DefaultBrowserPromoDeps.getInstance().getCurrentDefaultBrowserState(
                        createResolveInfo(
                                ContextUtils.getApplicationContext().getPackageName(), 1)));
    }

    @Test
    public void testIsChromePreStableInstalled() {
        List<ResolveInfo> infoList = new ArrayList<>();
        ShadowPackageManager packageManager =
                Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager());

        DefaultBrowserPromoDeps deps = DefaultBrowserPromoDeps.getInstance();
        infoList.add(createResolveInfo(DefaultBrowserPromoUtils.CHROME_STABLE_PACKAGE_NAME, 1));
        packageManager.addResolveInfoForIntent(
                PackageManagerUtils.getQueryInstalledBrowsersIntent(), infoList);
        Assert.assertFalse("Chrome stable should not be counted as a pre-stable channel",
                deps.isChromePreStableInstalled());

        infoList.add(createResolveInfo("com.android.chrome.123", 1));
        packageManager.addResolveInfoForIntent(
                PackageManagerUtils.getQueryInstalledBrowsersIntent(), infoList);
        Assert.assertFalse("A random package should not be counted as a pre-stable channel",
                deps.isChromePreStableInstalled());

        for (String name : DefaultBrowserPromoUtils.CHROME_PACKAGE_NAMES) {
            if (name.equals(DefaultBrowserPromoUtils.CHROME_STABLE_PACKAGE_NAME)) continue;
            List<ResolveInfo> list = new ArrayList<>(infoList);
            list.add(createResolveInfo(name, 1));
            packageManager.addResolveInfoForIntent(
                    PackageManagerUtils.getQueryInstalledBrowsersIntent(), list);
            Assert.assertTrue(name + " should be considered as a pre-stable channel",
                    deps.isChromePreStableInstalled());
        }
    }

    @Test
    public void testIsCurrentDefaultBrowserChrome() {
        DefaultBrowserPromoDeps deps = DefaultBrowserPromoDeps.getInstance();
        for (String name : DefaultBrowserPromoUtils.CHROME_PACKAGE_NAMES) {
            Assert.assertTrue(name + " should be considered as a chrome channel",
                    deps.isCurrentDefaultBrowserChrome(createResolveInfo(name, 1)));
        }

        Assert.assertFalse("A random string should not be considered as a chrome channel",
                deps.isCurrentDefaultBrowserChrome(
                        createResolveInfo("com.android.chrome.random.string", 1)));
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
