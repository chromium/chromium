// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.os.Build;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
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
    @Mock
    DefaultBrowserPromoDeps mDeps;

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
        // Setting android_manifest in the junit_binary build rule causes the current package to
        // appear in the PackageManager.
        packageManager.deletePackage(RuntimeEnvironment.application.getPackageName());

        DefaultBrowserPromoDeps deps = DefaultBrowserPromoDeps.getInstance();
        infoList.add(createResolveInfo(DefaultBrowserPromoDeps.CHROME_STABLE_PACKAGE_NAME, 1));
        packageManager.addResolveInfoForIntent(PackageManagerUtils.BROWSER_INTENT, infoList);
        Assert.assertFalse("Chrome stable should not be counted as a pre-stable channel",
                deps.isChromePreStableInstalled());

        infoList.add(createResolveInfo("com.android.chrome.123", 1));
        packageManager.addResolveInfoForIntent(PackageManagerUtils.BROWSER_INTENT, infoList);
        Assert.assertFalse("A random package should not be counted as a pre-stable channel",
                deps.isChromePreStableInstalled());

        for (String name : DefaultBrowserPromoDeps.CHROME_PACKAGE_NAMES) {
            if (name.equals(DefaultBrowserPromoDeps.CHROME_STABLE_PACKAGE_NAME)) continue;
            List<ResolveInfo> list = new ArrayList<>(infoList);
            list.add(createResolveInfo(name, 1));
            packageManager.addResolveInfoForIntent(PackageManagerUtils.BROWSER_INTENT, list);
            Assert.assertTrue(name + " should be considered as a pre-stable channel",
                    deps.isChromePreStableInstalled());
        }
    }

    @Test
    public void testIsCurrentDefaultBrowserChrome() {
        DefaultBrowserPromoDeps deps = DefaultBrowserPromoDeps.getInstance();
        for (String name : DefaultBrowserPromoDeps.CHROME_PACKAGE_NAMES) {
            Assert.assertTrue(name + " should be considered as a chrome channel",
                    deps.isCurrentDefaultBrowserChrome(createResolveInfo(name, 1)));
        }

        Assert.assertFalse("A random string should not be considered as a chrome channel",
                deps.isCurrentDefaultBrowserChrome(
                        createResolveInfo("com.android.chrome.random.string", 1)));
    }

    @Test
    public void testBasicPromo() {
        setDepsMockWithDefaultValues();
        Assert.assertTrue("Should promo disambiguation sheet on Q.",
                DefaultBrowserPromoUtils.shouldShowPromo(mDeps, null, false));
    }

    // --- Q above ---
    @Test
    public void testPromo_Q_No_Default() {
        setDepsMockWithDefaultValues();
        when(mDeps.isRoleAvailable(any())).thenReturn(true);
        Assert.assertTrue("Should promo role manager when there is no default browser on Q+.",
                DefaultBrowserPromoUtils.shouldShowPromo(mDeps, null, false));
    }

    @Test
    public void testPromo_Q_Other_Default() {
        setDepsMockWithDefaultValues();
        when(mDeps.isRoleAvailable(any())).thenReturn(true);
        when(mDeps.getDefaultWebBrowserActivityResolveInfo())
                .thenReturn(createResolveInfo("android", 1));
        Assert.assertTrue("Should promo role manager when there is another default browser on Q+.",
                DefaultBrowserPromoUtils.shouldShowPromo(mDeps, null, false));
    }

    // --- P below ---
    @Test
    public void testNoPromo_P() {
        setDepsMockWithDefaultValues();
        when(mDeps.getSDKInt()).thenReturn(Build.VERSION_CODES.P);
        when(mDeps.isRoleAvailable(any())).thenCallRealMethod();
        Assert.assertFalse("Should not promo on P-.",
                DefaultBrowserPromoUtils.shouldShowPromo(mDeps, null, false));
    }

    // --- prerequisites ---
    @Test
    public void testPromo_increasedPromoCount() {
        setDepsMockWithDefaultValues();
        when(mDeps.getMaxPromoCount()).thenReturn(100);
        when(mDeps.getPromoCount()).thenReturn(99);
        Assert.assertTrue("Should promo when promo count does not reach the upper limit.",
                DefaultBrowserPromoUtils.shouldShowPromo(mDeps, null, false));
    }

    @Test
    public void testNoPromo_greaterThanMaxPromoCount() {
        setDepsMockWithDefaultValues();
        when(mDeps.getPromoCount()).thenReturn(1);
        when(mDeps.getMaxPromoCount()).thenReturn(1);
        Assert.assertFalse("Should not promo when promo count reaches the upper limit.",
                DefaultBrowserPromoUtils.shouldShowPromo(mDeps, null, false));
    }

    @Test
    public void testPromo_ignoreMaxCount() {
        setDepsMockWithDefaultValues();
        when(mDeps.getPromoCount()).thenReturn(1);
        when(mDeps.getMaxPromoCount()).thenReturn(1);
        when(mDeps.getSessionCount()).thenReturn(1);
        when(mDeps.getMinSessionCount()).thenReturn(3);
        Assert.assertTrue("Should promo when ignore max count is enabled.",
                DefaultBrowserPromoUtils.shouldShowPromo(mDeps, null, true));
    }

    @Test
    public void testNoPromo_featureDisabled() {
        setDepsMockWithDefaultValues();
        when(mDeps.isFeatureEnabled()).thenReturn(false);
        Assert.assertFalse("Should not promo when the feature is disabled.",
                DefaultBrowserPromoUtils.shouldShowPromo(mDeps, null, false));
    }

    @Test
    public void testNoPromo_lessThanMinSessionCount() {
        setDepsMockWithDefaultValues();
        when(mDeps.getSessionCount()).thenReturn(1);
        when(mDeps.getMinSessionCount()).thenReturn(3);
        Assert.assertFalse(
                "Should not promo when session count has not reached the required amount.",
                DefaultBrowserPromoUtils.shouldShowPromo(mDeps, null, false));
    }

    @Test
    public void testNoPromo_isOtherChromeDefault() {
        setDepsMockWithDefaultValues();
        when(mDeps.getDefaultWebBrowserActivityResolveInfo())
                .thenReturn(
                        createResolveInfo(DefaultBrowserPromoDeps.CHROME_STABLE_PACKAGE_NAME, 1));
        when(mDeps.isCurrentDefaultBrowserChrome(any())).thenCallRealMethod();
        Assert.assertFalse("Should not promo when another chrome channel browser has been default.",
                DefaultBrowserPromoUtils.shouldShowPromo(mDeps, null, false));
    }

    @Test
    public void testNoPromo_isCurrentChromeDefault() {
        setDepsMockWithDefaultValues();
        when(mDeps.getDefaultWebBrowserActivityResolveInfo())
                .thenReturn(createResolveInfo(
                        ContextUtils.getApplicationContext().getPackageName(), 1));
        Assert.assertFalse("Should not promo when chrome has been default.",
                DefaultBrowserPromoUtils.shouldShowPromo(mDeps, null, false));
    }

    @Test
    public void testNoPromo_webBrowserActivityNotExist() {
        setDepsMockWithDefaultValues();
        when(mDeps.getDefaultWebBrowserActivityResolveInfo()).thenReturn(null);
        Assert.assertFalse("Should not promo when web browser activity does not exist.",
                DefaultBrowserPromoUtils.shouldShowPromo(mDeps, null, false));
    }

    private void setDepsMockWithDefaultValues() {
        when(mDeps.isFeatureEnabled()).thenReturn(true);
        when(mDeps.getMinSessionCount()).thenReturn(3);
        when(mDeps.getSessionCount()).thenReturn(10);
        when(mDeps.doesManageDefaultAppsSettingsActivityExist()).thenReturn(true);
        when(mDeps.getSDKInt()).thenReturn(Build.VERSION_CODES.Q);
        when(mDeps.isChromeStable()).thenReturn(false);
        when(mDeps.getPromoCount()).thenReturn(0);
        when(mDeps.getMaxPromoCount()).thenReturn(1);
        when(mDeps.getLastPromoInterval()).thenReturn(1000);
        when(mDeps.getMinPromoInterval()).thenReturn(10);
        when(mDeps.isChromePreStableInstalled()).thenReturn(false);
        when(mDeps.isCurrentDefaultBrowserChrome(any())).thenReturn(false);
        when(mDeps.isRoleAvailable(any())).thenReturn(true);
        // No Default
        when(mDeps.getDefaultWebBrowserActivityResolveInfo())
                .thenReturn(createResolveInfo("android", 0));
        when(mDeps.getCurrentDefaultBrowserState(any())).thenCallRealMethod();
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
