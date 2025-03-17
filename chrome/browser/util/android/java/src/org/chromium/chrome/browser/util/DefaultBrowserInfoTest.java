// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.chrome.browser.util.DefaultBrowserInfo.DefaultBrowserState;
import org.chromium.chrome.browser.util.DefaultBrowserInfo.DefaultInfo;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Unit test for {@link DefaultBrowserInfoTest}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class DefaultBrowserInfoTest {
    private TestContext mTestContext;
    private CallbackHelper mCallbackHelper;

    private static class TestContext extends ContextWrapper {
        private ResolveInfo mDefaultBrowser;
        private List<ResolveInfo> mAllBrowsers = new ArrayList<>();

        public TestContext(Context baseContext) {
            super(baseContext);
        }

        public void addBrowser(ResolveInfo ri, boolean isDefault) {
            mAllBrowsers.add(ri);
            if (isDefault) {
                mDefaultBrowser = ri;
            }
        }

        @Override
        public String getPackageName() {
            return DefaultBrowserInfo.CHROME_STABLE_PACKAGE_NAME;
        }

        @Override
        public PackageManager getPackageManager() {
            return new PackageManagerWrapper(super.getPackageManager()) {
                @Override
                public List<ResolveInfo> queryIntentActivities(Intent intent, int flags) {
                    return mAllBrowsers;
                }

                @Override
                public ResolveInfo resolveActivity(Intent intent, int flags) {
                    return mDefaultBrowser;
                }
            };
        }
    }

    @Before
    public void setUp() {
        mTestContext = new TestContext(ContextUtils.getApplicationContext());
        ContextUtils.initApplicationContextForTests(mTestContext);
        mCallbackHelper = new CallbackHelper();
        DefaultBrowserInfo.resetDefaultInfoTask();
    }

    private ResolveInfo createResolveInfo(String packageName, boolean isSystem) {
        ApplicationInfo applicationInfo = new ApplicationInfo();
        applicationInfo.packageName = packageName;
        applicationInfo.flags = isSystem ? ApplicationInfo.FLAG_SYSTEM : 0;

        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = packageName;
        activityInfo.applicationInfo = applicationInfo;

        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = activityInfo;
        resolveInfo.match = 1;
        return resolveInfo;
    }

    @Test
    @MediumTest
    public void testGetDefaultInfo_NoDefault() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        DefaultBrowserInfo.getDefaultBrowserInfo(
                                (info) -> {
                                    Assert.assertEquals(
                                            DefaultBrowserState.NO_DEFAULT,
                                            info.defaultBrowserState);
                                    Assert.assertFalse(info.isChromeSystem);
                                    Assert.assertFalse(info.isDefaultSystem);
                                    Assert.assertEquals(0, info.browserCount);
                                    Assert.assertEquals(0, info.systemCount);
                                    Assert.assertFalse(info.isChromePreStableInstalled);
                                    mCallbackHelper.notifyCalled();
                                }));
        mCallbackHelper.waitForCallback(0);
    }

    @Test
    @MediumTest
    public void testGetDefaultInfo_ChromeDefault() throws TimeoutException {
        ResolveInfo ri =
                createResolveInfo(
                        DefaultBrowserInfo.CHROME_STABLE_PACKAGE_NAME, /* isSystem= */ true);
        mTestContext.addBrowser(ri, /* isDefault= */ true);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        DefaultBrowserInfo.getDefaultBrowserInfo(
                                (info) -> {
                                    Assert.assertEquals(
                                            DefaultBrowserState.CHROME_DEFAULT,
                                            info.defaultBrowserState);
                                    Assert.assertTrue(info.isChromeSystem);
                                    Assert.assertTrue(info.isDefaultSystem);
                                    Assert.assertEquals(1, info.browserCount);
                                    Assert.assertEquals(1, info.systemCount);
                                    Assert.assertFalse(info.isChromePreStableInstalled);
                                    mCallbackHelper.notifyCalled();
                                }));
        mCallbackHelper.waitForCallback(0);
    }

    @Test
    @MediumTest
    public void testGetDefaultInfo_OtherChromeDefault() throws TimeoutException {
        for (String packageName : DefaultBrowserInfo.CHROME_PRE_STABLE_PACKAGE_NAMES) {
            mTestContext.addBrowser(
                    createResolveInfo(packageName, /* isSystem= */ false), /* isDefault= */ true);
            int callbackCount = mCallbackHelper.getCallCount();
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            DefaultBrowserInfo.getDefaultBrowserInfo(
                                    (info) -> {
                                        Assert.assertEquals(
                                                DefaultBrowserState.OTHER_CHROME_DEFAULT,
                                                info.defaultBrowserState);
                                        Assert.assertFalse(info.isChromeSystem);
                                        Assert.assertFalse(info.isDefaultSystem);
                                        Assert.assertEquals(0, info.systemCount);
                                        Assert.assertTrue(info.isChromePreStableInstalled);
                                        mCallbackHelper.notifyCalled();
                                    }));
            mCallbackHelper.waitForCallback(callbackCount);
        }
    }

    @Test
    @MediumTest
    public void testGetDefaultInfo_OtherDefault() throws TimeoutException {
        mTestContext.addBrowser(
                createResolveInfo(
                        DefaultBrowserInfo.CHROME_STABLE_PACKAGE_NAME, /* isSystem= */ false),
                /* isDefault= */ false);

        mTestContext.addBrowser(
                createResolveInfo("other.browser.package", /* isSystem= */ true),
                /* isDefault= */ true);

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        DefaultBrowserInfo.getDefaultBrowserInfo(
                                (info) -> {
                                    Assert.assertEquals(
                                            DefaultBrowserState.OTHER_DEFAULT,
                                            info.defaultBrowserState);
                                    Assert.assertFalse(info.isChromeSystem);
                                    Assert.assertTrue(info.isDefaultSystem);
                                    Assert.assertEquals(2, info.browserCount);
                                    Assert.assertEquals(1, info.systemCount);
                                    Assert.assertFalse(info.isChromePreStableInstalled);
                                    mCallbackHelper.notifyCalled();
                                }));
        mCallbackHelper.waitForCallback(0);
    }

    /**
     * Test {@link DefaultBrowserInfo#getDefaultBrowserInfo} will not fetch the default info again
     * if task is finished.
     */
    @Test
    @MediumTest
    public void testGetDefaultInfo_SameTask() throws TimeoutException {
        mTestContext.addBrowser(
                createResolveInfo(
                        DefaultBrowserInfo.CHROME_STABLE_PACKAGE_NAME, /* isSystem= */ true),
                /* isDefault= */ true);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        DefaultBrowserInfo.getDefaultBrowserInfo(
                                (info) -> {
                                    Assert.assertEquals(
                                            DefaultBrowserState.CHROME_DEFAULT,
                                            info.defaultBrowserState);
                                    Assert.assertEquals(1, info.browserCount);
                                    mCallbackHelper.notifyCalled();
                                }));
        mCallbackHelper.waitForCallback(0);

        // Set other package as default, and get default info again.
        mTestContext.addBrowser(
                createResolveInfo("other.browser.package", /* isSystem= */ false),
                /* isDefault= */ true);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        DefaultBrowserInfo.getDefaultBrowserInfo(
                                (info) -> {
                                    // Default browser is still Chrome, all fields in default info
                                    // stays the same.
                                    Assert.assertEquals(
                                            DefaultBrowserState.CHROME_DEFAULT,
                                            info.defaultBrowserState);
                                    Assert.assertEquals(1, info.browserCount);
                                    mCallbackHelper.notifyCalled();
                                }));
        mCallbackHelper.waitForCallback(1);
    }

    /** Test {@link DefaultBrowserInfo#resetDefaultInfoTask} will reset the default task. */
    @Test
    @MediumTest
    public void testResetAndGetDefaultInfo() throws TimeoutException {
        mTestContext.addBrowser(
                createResolveInfo(
                        DefaultBrowserInfo.CHROME_STABLE_PACKAGE_NAME, /* isSystem= */ true),
                /* isDefault= */ true);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        DefaultBrowserInfo.getDefaultBrowserInfo(
                                (info) -> {
                                    Assert.assertEquals(
                                            DefaultBrowserState.CHROME_DEFAULT,
                                            info.defaultBrowserState);
                                    Assert.assertEquals(1, info.browserCount);
                                    mCallbackHelper.notifyCalled();
                                }));
        mCallbackHelper.waitForCallback(0);

        // Set other package as default, and reset and get default info again.
        mTestContext.addBrowser(
                createResolveInfo("other.browser.package", /* isSystem= */ false),
                /* isDefault= */ true);

        DefaultBrowserInfo.resetDefaultInfoTask();

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        DefaultBrowserInfo.getDefaultBrowserInfo(
                                (info) -> {
                                    // Default browser changed to other default.
                                    Assert.assertEquals(
                                            DefaultBrowserState.OTHER_DEFAULT,
                                            info.defaultBrowserState);
                                    Assert.assertEquals(2, info.browserCount);
                                    mCallbackHelper.notifyCalled();
                                }));
        mCallbackHelper.waitForCallback(1);
    }

    /**
     * Test {@link DefaultBrowserInfo#getDefaultBrowserInfoSync} returns the current default info
     */
    @Test
    @MediumTest
    public void testGetDefaultBrowserInfoSync() throws TimeoutException {
        mTestContext.addBrowser(
                createResolveInfo(
                        DefaultBrowserInfo.CHROME_STABLE_PACKAGE_NAME, /* isSystem= */ true),
                /* isDefault= */ true);

        // Run getDefaultBrowserInfo to get the default info and wait for the task to complete.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DefaultInfo defaultInfo = DefaultBrowserInfo.getDefaultBrowserInfoSync();
                    Assert.assertEquals(
                            DefaultBrowserState.CHROME_DEFAULT, defaultInfo.defaultBrowserState);
                    Assert.assertEquals(1, defaultInfo.browserCount);
                });
    }
}
