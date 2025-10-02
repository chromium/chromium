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

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.IntentCallback;
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
    private static final int A2A_TRANSACTION_OUTCOME_SUCCEED = 1;
    private static final int A2A_TRANSACTION_OUTCOME_CANCELED = 2;
    private static final int A2A_TRANSACTION_OUTCOME_FAILED = 3;
    private static final String A2A_TRANSACTION_OUTCOME = "A2A_TRANSACTION_OUTCOME";
    private static final String A2A_INTENT_ACTION_NAME =
            "org.chromium.intent.action.FACILITATED_PAYMENT";
    private static final String GOOGLE_WALLET_PACKAGE_NAME = "com.google.android.apps.walletnfcrel";
    private static final String GBOARD_PACKAGE_NAME = "com.google.android.inputmethod.latin";
    private static final String NON_GBOARD_PACKAGE_NAME = "com.other.ime";
    private static final long PIX_MIN_SUPPORTED_WALLET_VERSION = 932848136L;
    private static final String EMAIL = "user@example.com";
    private static final GURL PAYMENT_LINK =
            new GURL("https://www.itmx.co.th/facilitated-payment/prompt-pay");
    private static final String PAYMENT_LINK_SCHEME = "PromptPay";

    @Rule public MockitoRule mRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mMockWindowAndroid;
    @Mock private Context mMockContext;
    @Mock private PackageManagerDelegate mMockPackageManagerDelegate;
    @Mock private Drawable mMockDrawable;
    @Mock private PackageManager mMockPackageManager;
    @Mock private InputMethodManager mMockInputMethodManager;
    @Mock private InputMethodInfo mMockInputMethodInfo;
    @Mock private ContentResolver mMockContentResolver;

    @Before
    public void setUp() {
        when(mMockWindowAndroid.getContext()).thenReturn(new WeakReference<Context>(mMockContext));
        when(mMockContext.getPackageManager()).thenReturn(mMockPackageManager);
        when(mMockContext.getSystemService(InputMethodManager.class))
                .thenReturn(mMockInputMethodManager);
        when(mMockContext.getContentResolver()).thenReturn(mMockContentResolver);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testIsPixSupportAvailableViaGboard_UnsupportedAndroidVersion() {
        assertFalse(DeviceDelegate.isPixSupportAvailableViaGboard(mMockWindowAndroid));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.TIRAMISU)
    public void testIsPixSupportAvailableViaGboard_NullWindowAndroid_AndroidT() {
        assertFalse(DeviceDelegate.isPixSupportAvailableViaGboard(null));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.TIRAMISU)
    public void testIsPixSupportAvailableViaGboard_NullContext_AndroidT() {
        when(mMockWindowAndroid.getContext()).thenReturn(new WeakReference<>(null));

        assertFalse(DeviceDelegate.isPixSupportAvailableViaGboard(mMockWindowAndroid));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.TIRAMISU)
    public void testIsPixSupportAvailableViaGboard_GboardIsCurrentIme_AndroidT() {
        Settings.Secure.putString(
                mMockContentResolver,
                Settings.Secure.DEFAULT_INPUT_METHOD,
                GBOARD_PACKAGE_NAME + "/.ImeService");

        assertTrue(DeviceDelegate.isPixSupportAvailableViaGboard(mMockWindowAndroid));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.TIRAMISU)
    public void testIsPixSupportAvailableViaGboard_GboardIsNotCurrentIme_AndroidT() {
        Settings.Secure.putString(
                mMockContentResolver,
                Settings.Secure.DEFAULT_INPUT_METHOD,
                NON_GBOARD_PACKAGE_NAME + "/.ImeService");

        assertFalse(DeviceDelegate.isPixSupportAvailableViaGboard(mMockWindowAndroid));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testIsPixSupportAvailableViaGboard_GboardIsCurrentIme() {
        when(mMockInputMethodManager.getCurrentInputMethodInfo()).thenReturn(mMockInputMethodInfo);
        when(mMockInputMethodInfo.getPackageName()).thenReturn(GBOARD_PACKAGE_NAME);

        assertTrue(DeviceDelegate.isPixSupportAvailableViaGboard(mMockWindowAndroid));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testIsPixSupportAvailableViaGboard_GboardIsNotCurrentIme() {
        when(mMockInputMethodManager.getCurrentInputMethodInfo()).thenReturn(mMockInputMethodInfo);
        when(mMockInputMethodInfo.getPackageName()).thenReturn(NON_GBOARD_PACKAGE_NAME);

        assertFalse(DeviceDelegate.isPixSupportAvailableViaGboard(mMockWindowAndroid));
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
                        PAYMENT_LINK_SCHEME,
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
                        PAYMENT_LINK_SCHEME,
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
                        PAYMENT_LINK_SCHEME,
                        PAYMENT_LINK,
                        mMockWindowAndroid));
        verify(mMockWindowAndroid).showIntent(any(Intent.class), any(), any());
    }

    @Test
    public void testInvokePaymentApp_callbackLogsSucceeded() {
        // Corresponds to A2A_TRANSACTION_OUTCOME_SUCCEED.
        testInvokePaymentAppCallback(
                Activity.RESULT_OK, A2A_TRANSACTION_OUTCOME_SUCCEED, "Succeeded");
    }

    @Test
    public void testInvokePaymentApp_callbackLogsCanceled() {
        // Corresponds to A2A_TRANSACTION_OUTCOME_CANCELED.
        testInvokePaymentAppCallback(
                Activity.RESULT_OK, A2A_TRANSACTION_OUTCOME_CANCELED, "Canceled");
    }

    @Test
    public void testInvokePaymentApp_callbackLogsFailed() {
        // Corresponds to A2A_TRANSACTION_OUTCOME_FAILED.
        testInvokePaymentAppCallback(Activity.RESULT_OK, A2A_TRANSACTION_OUTCOME_FAILED, "Failed");
    }

    @Test
    public void testInvokePaymentApp_callbackLogsUnknown_withInvalidCode() {
        testInvokePaymentAppCallback(Activity.RESULT_OK, 99, "Unknown");
    }

    @Test
    public void testInvokePaymentApp_callbackLogsUnknown_withNoOutcome() {
        testInvokePaymentAppCallback(Activity.RESULT_OK, null, "Unknown");
    }

    @Test
    public void testInvokePaymentApp_callbackLogsActivityCanceled() {
        testInvokePaymentAppCallback(Activity.RESULT_CANCELED, null, "Canceled");
    }

    private void testInvokePaymentAppCallback(
            int resultCode, @Nullable Integer transactionOutcome, String expectedOutcome) {
        ArgumentCaptor<IntentCallback> intentCallbackCaptor =
                ArgumentCaptor.forClass(IntentCallback.class);
        when(mMockWindowAndroid.showIntent(
                        any(Intent.class), intentCallbackCaptor.capture(), any()))
                .thenReturn(true);

        assertTrue(
                DeviceDelegate.invokePaymentApp(
                        "com.example.app",
                        "com.example.app.Activity",
                        PAYMENT_LINK_SCHEME,
                        PAYMENT_LINK,
                        mMockWindowAndroid));

        Intent resultIntent = new Intent();
        if (transactionOutcome != null) {
            resultIntent.putExtra(A2A_TRANSACTION_OUTCOME, transactionOutcome);
        }

        String latencyHistogram = "FacilitatedPayments.A2A." + expectedOutcome + ".Latency";
        String schemeLatencyHistogram = latencyHistogram + ".PromptPay";
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(latencyHistogram)
                        .expectAnyRecord(schemeLatencyHistogram)
                        .build();

        // Capture and invoke the callback
        IntentCallback callback = intentCallbackCaptor.getValue();
        callback.onIntentCompleted(resultCode, resultIntent);

        // Verifies that the histograms were recorded. We don't assert on the value because
        // the latency is not deterministic in this test.
        watcher.assertExpected();
    }
}
