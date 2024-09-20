// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
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
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;

/** Unit test for {@link DefaultBrowserPromoUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.Q)
public class DefaultBrowserPromoUtilsTest {
    @Mock DefaultBrowserPromoImpressionCounter mCounter;
    @Mock DefaultBrowserStateProvider mProvider;

    DefaultBrowserPromoUtils mUtils;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mUtils = new DefaultBrowserPromoUtils(mCounter, mProvider);
        setDepsMockWithDefaultValues();
    }

    @Test
    public void testBasicPromo() {
        Assert.assertTrue(
                "Should promo disambiguation sheet on Q.",
                mUtils.shouldShowRoleManagerPromo(null, false));
        Assert.assertFalse(mUtils.shouldShowOtherPromo(null));
    }

    // --- Q above ---
    @Test
    public void testPromo_Q_No_Default() {
        Assert.assertTrue(
                "Should promo role manager when there is no default browser on Q+.",
                mUtils.shouldShowRoleManagerPromo(null, false));
        Assert.assertFalse(mUtils.shouldShowOtherPromo(null));
    }

    @Test
    public void testPromo_Q_Other_Default() {
        when(mProvider.getDefaultWebBrowserActivityResolveInfo())
                .thenReturn(createResolveInfo("android", 1));
        Assert.assertTrue(
                "Should promo role manager when there is another default browser on Q+.",
                mUtils.shouldShowRoleManagerPromo(null, false));
        Assert.assertFalse(mUtils.shouldShowOtherPromo(null));
    }

    // --- P below ---
    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testNoPromo_P() {
        when(mProvider.isRoleAvailable(any())).thenCallRealMethod();
        Assert.assertFalse(
                "Should not promo on P-.", mUtils.shouldShowRoleManagerPromo(null, false));
        Assert.assertTrue(mUtils.shouldShowOtherPromo(null));
    }

    // --- prerequisites ---
    @Test
    public void testPromo_increasedPromoCount() {
        when(mCounter.getMaxPromoCount()).thenReturn(100);
        when(mCounter.getPromoCount()).thenReturn(99);
        Assert.assertTrue(
                "Should promo when promo count does not reach the upper limit.",
                mUtils.shouldShowRoleManagerPromo(null, false));
        Assert.assertFalse(mUtils.shouldShowOtherPromo(null));
    }

    @Test
    public void testNoPromo_greaterThanMaxPromoCount() {
        when(mCounter.getPromoCount()).thenReturn(1);
        when(mCounter.getMaxPromoCount()).thenReturn(1);
        Assert.assertFalse(
                "Should not promo when promo count reaches the upper limit.",
                mUtils.shouldShowRoleManagerPromo(null, false));
        Assert.assertTrue(mUtils.shouldShowOtherPromo(null));
    }

    @Test
    public void testPromo_ignoreMaxCount() {
        when(mCounter.getPromoCount()).thenReturn(1);
        when(mCounter.getMaxPromoCount()).thenReturn(1);
        // when(mCounter.getSessionCount()).thenReturn(1);
        // when(mCounter.getMinSessionCount()).thenReturn(3);
        Assert.assertTrue(
                "Should promo when ignore max count is enabled.",
                mUtils.shouldShowRoleManagerPromo(null, true));
    }

    @Test
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_DEFAULT_BROWSER_PROMO})
    public void testNoPromo_featureDisabled() {
        Assert.assertFalse(
                "Should not promo when the feature is disabled.",
                mUtils.shouldShowRoleManagerPromo(null, false));
        Assert.assertTrue(mUtils.shouldShowOtherPromo(null));
    }

    @Test
    public void testNoPromo_lessThanMinSessionCount() {
        when(mCounter.getSessionCount()).thenReturn(1);
        when(mCounter.getMinSessionCount()).thenReturn(3);
        Assert.assertFalse(
                "Should not promo when session count has not reached the required amount.",
                mUtils.shouldShowRoleManagerPromo(null, false));
        Assert.assertFalse(mUtils.shouldShowOtherPromo(null));
    }

    @Test
    public void testNoPromo_isOtherChromeDefault() {
        when(mProvider.getDefaultWebBrowserActivityResolveInfo())
                .thenReturn(
                        createResolveInfo(
                                DefaultBrowserStateProvider.CHROME_STABLE_PACKAGE_NAME, 1));
        when(mProvider.isCurrentDefaultBrowserChrome(any())).thenCallRealMethod();
        Assert.assertFalse(
                "Should not promo when another chrome channel browser has been default.",
                mUtils.shouldShowRoleManagerPromo(null, false));
        Assert.assertFalse(mUtils.shouldShowOtherPromo(null));
    }

    @Test
    public void testNoPromo_isCurrentChromeDefault() {
        when(mProvider.getDefaultWebBrowserActivityResolveInfo())
                .thenReturn(
                        createResolveInfo(
                                ContextUtils.getApplicationContext().getPackageName(), 1));
        Assert.assertFalse(
                "Should not promo when chrome has been default.",
                mUtils.shouldShowRoleManagerPromo(null, false));
        Assert.assertFalse(mUtils.shouldShowOtherPromo(null));
    }

    @Test
    public void testNoPromo_webBrowserActivityNotExist() {
        when(mProvider.getDefaultWebBrowserActivityResolveInfo()).thenReturn(null);
        Assert.assertFalse(
                "Should not promo when web browser activity does not exist.",
                mUtils.shouldShowRoleManagerPromo(null, false));
        Assert.assertFalse(mUtils.shouldShowOtherPromo(null));
    }

    private void setDepsMockWithDefaultValues() {
        when(mCounter.shouldShowPromo(anyBoolean())).thenCallRealMethod();
        when(mCounter.getMinSessionCount()).thenReturn(3);
        when(mCounter.getSessionCount()).thenReturn(10);
        when(mCounter.getPromoCount()).thenReturn(0);
        when(mCounter.getMaxPromoCount()).thenReturn(1);
        when(mCounter.getLastPromoInterval()).thenReturn(1000);
        when(mCounter.getMinPromoInterval()).thenReturn(10);

        when(mProvider.shouldShowPromo()).thenCallRealMethod();
        when(mProvider.isChromeStable()).thenReturn(false);
        when(mProvider.isChromePreStableInstalled()).thenReturn(false);
        when(mProvider.isCurrentDefaultBrowserChrome(any())).thenReturn(false);
        when(mProvider.isRoleAvailable(any())).thenReturn(true);
        // No Default
        when(mProvider.getDefaultWebBrowserActivityResolveInfo())
                .thenReturn(createResolveInfo("android", 0));
        when(mProvider.getCurrentDefaultBrowserState(any())).thenCallRealMethod();
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
