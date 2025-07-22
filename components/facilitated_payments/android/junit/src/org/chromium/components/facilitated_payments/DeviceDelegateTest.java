// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.facilitated_payments;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Tests for {@link DeviceDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@SmallTest
public class DeviceDelegateTest {
    private static final String A2A_INTENT_ACTION_NAME =
            "org.chromium.intent.action.FACILITATED_PAYMENT";
    private static final String GOOGLE_WALLET_PACKAGE_NAME = "com.google.android.apps.walletnfcrel";
    private static final String EMAIL = "user@example.com";
    private static final GURL PAYMENT_LINK = new GURL("https://www.example.com");

    @Rule public MockitoRule mRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mMockWindowAndroid;
    @Mock private Context mMockContext;
    @Mock private PackageManagerDelegate mMockPackageManagerDelegate;
    @Mock private Drawable mMockDrawable;
    @Mock private PackageManager mMockPackageManager;

    @Before
    public void setUp() {
        when(mMockWindowAndroid.getContext()).thenReturn(new WeakReference<Context>(mMockContext));
        when(mMockContext.getPackageManager()).thenReturn(mMockPackageManager);
    }

    @Test
    public void testOpenPixAccountLinkingPageInWallet_Success() {
        DeviceDelegate.openPixAccountLinkingPageInWallet(mMockWindowAndroid, EMAIL);

        // Capture the Intent passed to startActivity
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockContext).startActivity(intentCaptor.capture());

        // Assert the properties of the captured Intent
        Intent capturedIntent = intentCaptor.getValue();
        assertEquals(Intent.ACTION_VIEW, capturedIntent.getAction());
        assertEquals(
                Uri.parse(
                        "https://wallet.google.com/gw/app/addbankaccount?utm_source=chrome&email=user@example.com"),
                capturedIntent.getData());
        assertEquals(GOOGLE_WALLET_PACKAGE_NAME, capturedIntent.getPackage());
    }

    @Test
    public void testOpenPixAccountLinkingPageInWallet_NullWindowAndroid() {
        DeviceDelegate.openPixAccountLinkingPageInWallet(null, EMAIL);

        // Verify that startActivity() was never called if WindowAndroid is null.
        verify(mMockContext, never()).startActivity(any(Intent.class));
    }

    @Test
    public void testOpenPixAccountLinkingPageInWallet_NullContext() {
        when(mMockWindowAndroid.getContext()).thenReturn(new WeakReference<>(null));

        DeviceDelegate.openPixAccountLinkingPageInWallet(mMockWindowAndroid, EMAIL);

        verify(mMockContext, never()).startActivity(any(Intent.class));
    }

    @Test
    public void testOpenPixAccountLinkingPageInWallet_ActivityNotFound() {
        // Simulate ActivityNotFoundException
        doThrow(new ActivityNotFoundException())
                .when(mMockContext)
                .startActivity(any(Intent.class));

        // Call the method, expecting it to catch the exception
        DeviceDelegate.openPixAccountLinkingPageInWallet(mMockWindowAndroid, EMAIL);

        // Verify startActivity was called (even though it threw).
        verify(mMockContext).startActivity(any(Intent.class));
    }

    @Test
    public void testGetSupportedPaymentApps_nullWindowAndroid() {
        ResolveInfo[] apps = DeviceDelegate.getSupportedPaymentApps(PAYMENT_LINK, null);

        assertEquals(0, apps.length);
    }

    @Test
    public void testGetSupportedPaymentApps_nullContext() {
        when(mMockWindowAndroid.getContext()).thenReturn(new WeakReference<>(null));

        ResolveInfo[] apps =
                DeviceDelegate.getSupportedPaymentApps(PAYMENT_LINK, mMockWindowAndroid);

        assertEquals(0, apps.length);
    }

    @Test
    public void testGetSupportedPaymentApps_nullPackageManager() {
        when(mMockContext.getPackageManager()).thenReturn(null);

        ResolveInfo[] apps =
                DeviceDelegate.getSupportedPaymentApps(PAYMENT_LINK, mMockWindowAndroid);

        assertEquals(0, apps.length);
    }

    @Test
    public void testGetSupportedPaymentApps_noActivitiesFound() {
        when(mMockPackageManagerDelegate.getActivitiesThatCanRespondToIntent(any(Intent.class)))
                .thenReturn(Collections.emptyList());

        ResolveInfo[] apps =
                DeviceDelegate.getSupportedPaymentApps(
                        PAYMENT_LINK, mMockWindowAndroid, mMockPackageManagerDelegate);

        assertEquals(0, apps.length);
        verify(mMockPackageManagerDelegate).getActivitiesThatCanRespondToIntent(any(Intent.class));
    }

    @Test
    public void testGetSupportedPaymentApps_oneValidApp() {
        ResolveInfo validApp = createResolveInfo("com.valid.app", "ValidAppActivity");
        when(mMockPackageManagerDelegate.getActivitiesThatCanRespondToIntent(any(Intent.class)))
                .thenReturn(Collections.singletonList(validApp));
        when(mMockPackageManagerDelegate.getAppLabel(validApp)).thenReturn("Valid App");
        when(mMockPackageManagerDelegate.getAppIcon(validApp)).thenReturn(mMockDrawable);

        ResolveInfo[] apps =
                DeviceDelegate.getSupportedPaymentApps(
                        PAYMENT_LINK, mMockWindowAndroid, mMockPackageManagerDelegate);

        assertEquals(1, apps.length);
        assertEquals("com.valid.app", apps[0].activityInfo.packageName);
    }

    @Test
    public void testGetSupportedPaymentApps_multipleValidApps() {
        ResolveInfo validApp1 = createResolveInfo("com.valid.app1", "ValidApp1Activity");
        ResolveInfo validApp2 = createResolveInfo("com.valid.app2", "ValidApp2Activity");
        when(mMockPackageManagerDelegate.getActivitiesThatCanRespondToIntent(any(Intent.class)))
                .thenReturn(Arrays.asList(validApp1, validApp2));
        when(mMockPackageManagerDelegate.getAppLabel(validApp1)).thenReturn("Valid App 1");
        when(mMockPackageManagerDelegate.getAppIcon(validApp1)).thenReturn(mMockDrawable);
        when(mMockPackageManagerDelegate.getAppLabel(validApp2)).thenReturn("Valid App 2");
        when(mMockPackageManagerDelegate.getAppIcon(validApp2)).thenReturn(mMockDrawable);

        ResolveInfo[] apps =
                DeviceDelegate.getSupportedPaymentApps(
                        PAYMENT_LINK, mMockWindowAndroid, mMockPackageManagerDelegate);

        assertEquals(2, apps.length);
        List<String> packageNames = new ArrayList<>();
        for (ResolveInfo app : apps) {
            packageNames.add(app.activityInfo.packageName);
        }
        assertTrue(packageNames.contains("com.valid.app1"));
        assertTrue(packageNames.contains("com.valid.app2"));
    }

    @Test
    public void testGetSupportedPaymentApps_deduplicatesApps() {
        ResolveInfo app1 = createResolveInfo("com.valid.app", "Activity1");
        ResolveInfo app2 = createResolveInfo("com.valid.app", "Activity2"); // Same package
        when(mMockPackageManagerDelegate.getActivitiesThatCanRespondToIntent(any(Intent.class)))
                .thenReturn(Arrays.asList(app1, app2));
        when(mMockPackageManagerDelegate.getAppLabel(any(ResolveInfo.class)))
                .thenReturn("Same App");
        when(mMockPackageManagerDelegate.getAppIcon(any(ResolveInfo.class)))
                .thenReturn(mMockDrawable);

        ResolveInfo[] apps =
                DeviceDelegate.getSupportedPaymentApps(
                        PAYMENT_LINK, mMockWindowAndroid, mMockPackageManagerDelegate);

        assertEquals(1, apps.length);
        assertEquals("com.valid.app", apps[0].activityInfo.packageName);
    }

    @Test
    public void testGetSupportedPaymentApps_filtersInvalidApps() {
        ResolveInfo validApp = createResolveInfo("com.valid.app", "ValidAppActivity");
        ResolveInfo appWithNullActivityInfo = createResolveInfo("com.null.activity", "Activity");
        appWithNullActivityInfo.activityInfo = null;
        ResolveInfo appWithNullPackage = createResolveInfo(null, "Activity");
        ResolveInfo appWithEmptyPackage = createResolveInfo("", "Activity");
        ResolveInfo appWithNullName = createResolveInfo("com.null.name", null);
        ResolveInfo appWithEmptyName = createResolveInfo("com.empty.name", "");
        ResolveInfo appWithNullLabel = createResolveInfo("com.null.label", "Activity");
        ResolveInfo appWithEmptyLabel = createResolveInfo("com.empty.label", "Activity");
        ResolveInfo appWithNullIcon = createResolveInfo("com.null.icon", "Activity");

        List<ResolveInfo> allApps =
                Arrays.asList(
                        validApp,
                        appWithNullActivityInfo,
                        appWithNullPackage,
                        appWithEmptyPackage,
                        appWithNullName,
                        appWithEmptyName,
                        appWithNullLabel,
                        appWithEmptyLabel,
                        appWithNullIcon);

        when(mMockPackageManagerDelegate.getActivitiesThatCanRespondToIntent(any(Intent.class)))
                .thenReturn(allApps);

        // Setup mocks for valid and invalid apps
        when(mMockPackageManagerDelegate.getAppLabel(validApp)).thenReturn("Valid App");
        when(mMockPackageManagerDelegate.getAppIcon(validApp)).thenReturn(mMockDrawable);

        when(mMockPackageManagerDelegate.getAppLabel(appWithNullLabel)).thenReturn(null);
        when(mMockPackageManagerDelegate.getAppLabel(appWithEmptyLabel)).thenReturn(" ");

        when(mMockPackageManagerDelegate.getAppLabel(appWithNullIcon))
                .thenReturn("App With Null Icon");
        when(mMockPackageManagerDelegate.getAppIcon(appWithNullIcon)).thenReturn(null);

        ResolveInfo[] apps =
                DeviceDelegate.getSupportedPaymentApps(
                        PAYMENT_LINK, mMockWindowAndroid, mMockPackageManagerDelegate);

        assertEquals(1, apps.length);
        assertEquals("com.valid.app", apps[0].activityInfo.packageName);
    }

    private ResolveInfo createResolveInfo(String packageName, String name) {
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.packageName = packageName;
        resolveInfo.activityInfo.name = name;
        return resolveInfo;
    }

    @Test
    public void testNullWindowAndroidCannotInvokePaymentApp() {
        assertFalse(
                DeviceDelegate.invokePaymentApp(
                        "com.example.app",
                        "com.example.app.Activity",
                        PAYMENT_LINK,
                        /* windowAndroid= */ null));
    }

    @Test
    public void testInvokePaymentApp_launchesIntentSuccessfully() {
        when(mMockWindowAndroid.showIntent(any(Intent.class), any(), any())).thenReturn(true);

        assertTrue(
                DeviceDelegate.invokePaymentApp(
                        "com.example.app",
                        "com.example.app.Activity",
                        PAYMENT_LINK,
                        mMockWindowAndroid));

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockWindowAndroid).showIntent(intentCaptor.capture(), any(), any());

        Intent capturedIntent = intentCaptor.getValue();
        assertEquals(A2A_INTENT_ACTION_NAME, capturedIntent.getAction());
        assertEquals(Uri.parse(PAYMENT_LINK.getSpec()), capturedIntent.getData());
        assertEquals("com.example.app", capturedIntent.getComponent().getPackageName());
        assertEquals("com.example.app.Activity", capturedIntent.getComponent().getClassName());
    }

    @Test
    public void testInvokePaymentApp_showIntentReturnsFalse() {
        when(mMockWindowAndroid.showIntent(any(Intent.class), any(), any())).thenReturn(false);

        assertFalse(
                DeviceDelegate.invokePaymentApp(
                        "com.example.app",
                        "com.example.app.Activity",
                        PAYMENT_LINK,
                        mMockWindowAndroid));
        verify(mMockWindowAndroid).showIntent(any(Intent.class), any(), any());
    }
}
