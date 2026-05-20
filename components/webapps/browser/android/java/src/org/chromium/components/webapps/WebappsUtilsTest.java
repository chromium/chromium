// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.robolectric.Shadows.shadowOf;

import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.graphics.Bitmap;
import android.os.Build;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowPackageManager;
import org.robolectric.shadows.ShadowShortcutManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.List;

/** Tests WebappsUtils. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.Q)
public class WebappsUtilsTest {
    private Context mContext;
    private ShortcutManager mShortcutManager;
    private ShadowShortcutManager mShadowShortcutManager;
    private ShadowPackageManager mShadowPackageManager;
    private ShadowApplication mShadowApplication;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        ContextUtils.initApplicationContextForTests(mContext);
        mShortcutManager = mContext.getSystemService(ShortcutManager.class);
        mShadowShortcutManager = shadowOf(mShortcutManager);
        mShadowPackageManager = shadowOf(mContext.getPackageManager());
        mShadowApplication = shadowOf((Application) mContext);
        WebappsUtils.setAddToHomeIntentSupportedForTesting(null); // Reset
    }

    @Test
    @SmallTest
    @Feature({"Webapp"})
    public void testIsAddToHomeIntentSupported_ShortcutManagerSupported() {
        mShadowShortcutManager.setIsRequestPinShortcutSupported(true);
        assertTrue(WebappsUtils.isAddToHomeIntentSupported());
    }

    @Test
    @SmallTest
    @Feature({"Webapp"})
    public void testIsAddToHomeIntentSupported_ShortcutManagerNotSupported() {
        mShadowShortcutManager.setIsRequestPinShortcutSupported(false);
        assertFalse(WebappsUtils.isAddToHomeIntentSupported());
    }

    @Test
    @SmallTest
    @Feature({"Webapp"})
    public void testAddShortcutToHomescreen_ShortcutManagerSupported() {
        mShadowShortcutManager.setIsRequestPinShortcutSupported(true);
        Bitmap icon = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        Intent intent = new Intent(Intent.ACTION_VIEW);

        WebappsUtils.addShortcutToHomescreen("id", "title", icon, false, intent);

        List<ShortcutInfo> shortcuts = mShortcutManager.getPinnedShortcuts();
        assertEquals(1, shortcuts.size());
        assertEquals("id", shortcuts.get(0).getId());
        assertEquals("title", shortcuts.get(0).getShortLabel());
    }

    @Test
    @SmallTest
    @Feature({"Webapp"})
    public void testAddShortcutToHomescreen_ShortcutManagerNotSupported() {
        mShadowShortcutManager.setIsRequestPinShortcutSupported(false);
        Bitmap icon = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        Intent intent = new Intent(Intent.ACTION_VIEW);

        WebappsUtils.addShortcutToHomescreen("id", "title", icon, false, intent);

        List<ShortcutInfo> shortcuts = mShortcutManager.getPinnedShortcuts();
        assertEquals(0, shortcuts.size());
        assertTrue(mShadowApplication.getBroadcastIntents().isEmpty());
    }

    @Test
    @SmallTest
    @Feature({"Webapp"})
    public void
            testIsAddToHomeIntentSupported_ShortcutManagerNotSupported_HasDefaultLauncherWithFallback() {
        mShadowShortcutManager.setIsRequestPinShortcutSupported(false);

        String launcherPackage = "com.example.launcher";
        Intent homeIntent = new Intent(Intent.ACTION_MAIN);
        homeIntent.addCategory(Intent.CATEGORY_HOME);
        ResolveInfo homeResolveInfo = new ResolveInfo();
        homeResolveInfo.activityInfo = new ActivityInfo();
        homeResolveInfo.activityInfo.packageName = launcherPackage;
        homeResolveInfo.activityInfo.name = "LauncherActivity";
        mShadowPackageManager.addResolveInfoForIntent(homeIntent, homeResolveInfo);

        Intent installIntent = new Intent("com.android.launcher.action.INSTALL_SHORTCUT");
        installIntent.setPackage(launcherPackage);
        ResolveInfo receiverResolveInfo = new ResolveInfo();
        receiverResolveInfo.activityInfo = new ActivityInfo();
        receiverResolveInfo.activityInfo.packageName = launcherPackage;
        receiverResolveInfo.activityInfo.name = "InstallShortcutReceiver";
        mShadowPackageManager.addResolveInfoForIntent(installIntent, receiverResolveInfo);

        assertTrue(WebappsUtils.isAddToHomeIntentSupported());
    }

    @Test
    @SmallTest
    @Feature({"Webapp"})
    public void
            testIsAddToHomeIntentSupported_ShortcutManagerNotSupported_DefaultLauncherNoFallback() {
        mShadowShortcutManager.setIsRequestPinShortcutSupported(false);

        String launcherPackage = "com.example.launcher";
        Intent homeIntent = new Intent(Intent.ACTION_MAIN);
        homeIntent.addCategory(Intent.CATEGORY_HOME);
        ResolveInfo homeResolveInfo = new ResolveInfo();
        homeResolveInfo.activityInfo = new ActivityInfo();
        homeResolveInfo.activityInfo.packageName = launcherPackage;
        homeResolveInfo.activityInfo.name = "LauncherActivity";
        mShadowPackageManager.addResolveInfoForIntent(homeIntent, homeResolveInfo);

        assertFalse(WebappsUtils.isAddToHomeIntentSupported());
    }

    @Test
    @SmallTest
    @Feature({"Webapp"})
    public void testAddShortcutToHomescreen_ShortcutManagerNotSupported_HasDefaultLauncher() {
        mShadowShortcutManager.setIsRequestPinShortcutSupported(false);

        String launcherPackage = "com.example.launcher";
        Intent homeIntent = new Intent(Intent.ACTION_MAIN);
        homeIntent.addCategory(Intent.CATEGORY_HOME);
        ResolveInfo homeResolveInfo = new ResolveInfo();
        homeResolveInfo.activityInfo = new ActivityInfo();
        homeResolveInfo.activityInfo.packageName = launcherPackage;
        homeResolveInfo.activityInfo.name = "LauncherActivity";
        mShadowPackageManager.addResolveInfoForIntent(homeIntent, homeResolveInfo);

        Bitmap icon = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        Intent intent = new Intent(Intent.ACTION_VIEW);

        WebappsUtils.addShortcutToHomescreen("id", "title", icon, false, intent);

        List<Intent> broadcasts = mShadowApplication.getBroadcastIntents();
        assertEquals(1, broadcasts.size());
        Intent broadcast = broadcasts.get(0);
        assertEquals("com.android.launcher.action.INSTALL_SHORTCUT", broadcast.getAction());
        assertEquals(launcherPackage, broadcast.getPackage());
        assertEquals("title", broadcast.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));
        assertEquals(intent, broadcast.getParcelableExtra(Intent.EXTRA_SHORTCUT_INTENT));
        assertEquals(icon, broadcast.getParcelableExtra(Intent.EXTRA_SHORTCUT_ICON));
    }
}
