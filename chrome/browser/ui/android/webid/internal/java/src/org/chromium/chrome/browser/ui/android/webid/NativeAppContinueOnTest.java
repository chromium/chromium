// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivity;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifierFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.content_relationship_verification.OriginVerifier.OriginVerificationListener;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.IntentCallback;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;

/** Robolectric tests for native app continue_on flow in AccountSelectionCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ContentFeatures.FED_CM_NATIVE_ID_PS)
public class NativeAppContinueOnTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String IDP_PACKAGE = "com.idp.app";
    private static final GURL CONTINUE_URL = new GURL("https://idp.com/continue");
    private static final Origin IDP_ORIGIN = Origin.create(CONTINUE_URL.getSpec());

    @Mock private Tab mTab;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private AccountSelectionComponent.Delegate mMockDelegate;

    private AccountSelectionCoordinator mCoordinator;
    private Activity mActivity;
    private Context mSpyContext;
    private PackageManager mSpyPackageManager;
    private ShadowActivity mShadowActivity;
    private ShadowPackageManager mShadowPackageManager;
    private ChromeOriginVerifier mMockOriginVerifier;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mSpyContext = spy(mActivity);
        mSpyPackageManager = spy(mActivity.getPackageManager());
        doReturn(mSpyPackageManager).when(mSpyContext).getPackageManager();

        mShadowActivity = Shadows.shadowOf(mActivity);
        mShadowPackageManager = Shadows.shadowOf(mActivity.getPackageManager());

        WeakReference<Context> contextRef = new WeakReference<>(mSpyContext);
        when(mWindowAndroid.getContext()).thenReturn(contextRef);
        WeakReference<Activity> activityRef = new WeakReference<>((Activity) mSpyContext);
        when(mWindowAndroid.getActivity()).thenReturn(activityRef);

        mMockOriginVerifier = mock(ChromeOriginVerifier.class);
        ChromeOriginVerifierFactory.setInstanceForTesting(mMockOriginVerifier);

        // Default stubbing: verification succeeds
        doAnswer(
                        invocation -> {
                            OriginVerificationListener listener = invocation.getArgument(0);
                            Origin origin = invocation.getArgument(1);
                            listener.onOriginVerified(IDP_PACKAGE, origin, true, true);
                            return null;
                        })
                .when(mMockOriginVerifier)
                .start(any(OriginVerificationListener.class), any(Origin.class));

        mCoordinator =
                new AccountSelectionCoordinator(
                        mTab,
                        mWindowAndroid,
                        mBottomSheetController,
                        0, // RpMode.PASSIVE
                        true, // canShowUi
                        mMockDelegate);
    }

    private void registerFakeApp(String packageName) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setData(Uri.parse(CONTINUE_URL.getSpec()));
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.packageName = packageName;
        resolveInfo.activityInfo.name = packageName + ".LoginActivity";
        mShadowPackageManager.addResolveInfoForIntent(intent, resolveInfo);
    }

    private void registerFakeApp(String packageName, String mimeType) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setDataAndType(Uri.parse(CONTINUE_URL.getSpec()), mimeType);
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.packageName = packageName;
        resolveInfo.activityInfo.name = packageName + ".LoginActivity";
        mShadowPackageManager.addResolveInfoForIntent(intent, resolveInfo);
    }

    @Test
    public void testNativeFlowIgnoresResult() {
        registerFakeApp(IDP_PACKAGE, "application/web-identity+json");

        // Mock WindowAndroid.showIntent to simulate success and return token
        doAnswer(
                        invocation -> {
                            Intent intent = invocation.getArgument(0);
                            IntentCallback callback = invocation.getArgument(1);

                            assertEquals(IDP_PACKAGE, intent.getPackage());
                            assertEquals(Intent.ACTION_VIEW, intent.getAction());
                            assertEquals(CONTINUE_URL.getSpec(), intent.getDataString());
                            assertEquals("application/web-identity+json", intent.getType());

                            // Simulate App returning result
                            Intent resultIntent = new Intent();
                            resultIntent.putExtra("token", "success_token");
                            callback.onIntentCompleted(Activity.RESULT_OK, resultIntent);
                            return true;
                        })
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(IntentCallback.class), any());

        mCoordinator.showModalDialog(CONTINUE_URL);

        // Verify that native app result is NOT propagated and it treats as
        // dismissed
        // TODO(crbug.com/521864267): add support for allowing the result to
        // propagate.
        verify(mMockDelegate).onDismissed(any(Integer.class));
        assertNull(mShadowActivity.getNextStartedActivity());
    }

    @Test
    public void testNativeFlowWithoutTokenTreatsAsDismissed() {
        registerFakeApp(IDP_PACKAGE, "application/web-identity+json");

        doAnswer(
                        invocation -> {
                            IntentCallback callback = invocation.getArgument(1);
                            callback.onIntentCompleted(Activity.RESULT_OK, new Intent());
                            return true;
                        })
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(IntentCallback.class), any());

        mCoordinator.showModalDialog(CONTINUE_URL);

        verify(mMockDelegate).onDismissed(any(Integer.class));
    }

    @Test
    public void testFallbackToCctWhenNoAppInstalled() {
        // No app registered

        mCoordinator.showModalDialog(CONTINUE_URL);

        // Verify CCT launch
        Intent intent = mShadowActivity.getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(mActivity.getPackageName(), intent.getPackage());
        assertEquals(CONTINUE_URL.getSpec(), intent.getDataString());
    }

    @Test
    @DisableFeatures(ContentFeatures.FED_CM_NATIVE_ID_PS)
    public void testFallbackToCctWhenFeatureDisabled() {
        registerFakeApp(IDP_PACKAGE);

        mCoordinator.showModalDialog(CONTINUE_URL);

        // Verify CCT launch (because feature is disabled, even though app is registered)
        Intent intent = mShadowActivity.getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(mActivity.getPackageName(), intent.getPackage());
        assertEquals(CONTINUE_URL.getSpec(), intent.getDataString());
    }

    @Test
    public void testNativeFlowWithMimeType() {
        registerFakeApp(IDP_PACKAGE, "application/web-identity+json");

        mCoordinator.showModalDialog(CONTINUE_URL);

        // Verify native app was launched with MIME type
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mWindowAndroid).showIntent(intentCaptor.capture(), any(IntentCallback.class), any());

        Intent intent = intentCaptor.getValue();
        assertNotNull(intent);
        assertEquals("application/web-identity+json", intent.getType());
        assertEquals(CONTINUE_URL.getSpec(), intent.getDataString());
        assertEquals(IDP_PACKAGE, intent.getPackage());
    }

    @Test
    public void testFallbackToCctWhenAppRegisteredWithoutMimeType() {
        // Register app without MIME type
        registerFakeApp(IDP_PACKAGE);

        // Stub package manager to return empty for MIME type queries (workaround Robolectric bug)
        doReturn(java.util.Collections.emptyList())
                .when(mSpyPackageManager)
                .queryIntentActivities(
                        argThat(
                                (Intent intent) ->
                                        "application/web-identity+json".equals(intent.getType())),
                        anyInt());

        mCoordinator.showModalDialog(CONTINUE_URL);

        // Verify native app was NOT launched
        verify(mWindowAndroid, never())
                .showIntent(any(Intent.class), any(IntentCallback.class), any());

        // Verify CCT launch fallback
        Intent intent = mShadowActivity.getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(mActivity.getPackageName(), intent.getPackage());
        assertEquals(CONTINUE_URL.getSpec(), intent.getDataString());
    }

    @Test
    public void testFallbackToCctWhenVerificationFails() {
        // Register app with MIME type to pass MIME type check
        registerFakeApp(IDP_PACKAGE, "application/web-identity+json");

        // Mock verification failure
        doAnswer(
                        invocation -> {
                            OriginVerificationListener listener = invocation.getArgument(0);
                            Origin origin = invocation.getArgument(1);
                            listener.onOriginVerified(IDP_PACKAGE, origin, false, true);
                            return null;
                        })
                .when(mMockOriginVerifier)
                .start(any(OriginVerificationListener.class), any(Origin.class));

        mCoordinator.showModalDialog(CONTINUE_URL);

        // Verify native app was NOT launched
        verify(mWindowAndroid, never())
                .showIntent(any(Intent.class), any(IntentCallback.class), any());

        // Verify CCT launch fallback
        Intent intent = mShadowActivity.getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(mActivity.getPackageName(), intent.getPackage());
        assertEquals(CONTINUE_URL.getSpec(), intent.getDataString());
    }

    @Test
    public void testNativeFlowTriesNextAppWhenFirstFailsVerification() {
        String pkg1 = "org.test.app1";
        String pkg2 = "org.test.app2";
        registerFakeApp(pkg1, "application/web-identity+json");
        registerFakeApp(pkg2, "application/web-identity+json");

        // Mock verification: 1st call fails, 2nd call succeeds
        doAnswer(
                        invocation -> {
                            OriginVerificationListener listener = invocation.getArgument(0);
                            Origin origin = invocation.getArgument(1);
                            listener.onOriginVerified(pkg1, origin, false, true);
                            return null;
                        })
                .doAnswer(
                        invocation -> {
                            OriginVerificationListener listener = invocation.getArgument(0);
                            Origin origin = invocation.getArgument(1);
                            listener.onOriginVerified(pkg2, origin, true, true);
                            return null;
                        })
                .when(mMockOriginVerifier)
                .start(any(OriginVerificationListener.class), any(Origin.class));

        // Mock showIntent to succeed
        when(mWindowAndroid.showIntent(any(Intent.class), any(IntentCallback.class), any()))
                .thenReturn(true);

        mCoordinator.showModalDialog(CONTINUE_URL);

        // Verify native app 2 was launched (since 1 failed verification)
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mWindowAndroid).showIntent(intentCaptor.capture(), any(IntentCallback.class), any());

        Intent intent = intentCaptor.getValue();
        assertNotNull(intent);
        assertEquals(pkg2, intent.getPackage());

        // Verify CCT was NOT launched
        Intent cctIntent = mShadowActivity.getNextStartedActivity();
        assertNull(cctIntent);
    }
}
