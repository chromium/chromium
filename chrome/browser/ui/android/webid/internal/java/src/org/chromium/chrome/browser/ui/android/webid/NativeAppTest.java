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
import org.robolectric.shadows.ShadowActivity;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.TriState;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifierFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.browser.ui.android.webid.data.NativeAppRequestOptions;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.content_relationship_verification.OriginVerifier.OriginVerificationListener;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.IntentCallback;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/** Robolectric tests for native app continue_on flow in AccountSelectionCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ContentFeatures.FED_CM_NATIVE_ID_PS,
    ChromeFeatureList.CCT_DONT_OVERRIDE_INTENT_MIME_TYPE
})
public class NativeAppTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String IDP_PACKAGE = "com.idp.app";
    private static final GURL CONTINUE_URL = new GURL("https://idp.com/continue");
    private static final GURL LOGIN_URL = new GURL("https://idp.com/login");
    // Matches the real output of ComputeUrlEncodedTokenPostData().
    private static final String ASSERTION_PARAMS =
            "client_id=client123&nonce=nonce456&disclosure_text_shown=false"
                    + "&is_auto_selected=false&mode=active&fields=name,email,picture";

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
                            listener.onOriginVerified(IDP_PACKAGE, origin, true, TriState.TRUE);
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
        registerFakeApp(packageName, mimeType, CONTINUE_URL);
    }

    private void registerFakeApp(String packageName, String mimeType, GURL url) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setDataAndType(Uri.parse(url.getSpec()), mimeType);
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.packageName = packageName;
        resolveInfo.activityInfo.name = packageName + ".LoginActivity";
        mShadowPackageManager.addResolveInfoForIntent(intent, resolveInfo);
    }

    /**
     * Tests that when a native app intent completes with RESULT_OK without a "token" extra in the
     * result intent data (indicating a login URL flow instead of continuation token flow),
     * onNativeAppLoginFinished() is triggered on the delegate.
     */
    @Test
    public void testNativeFlowLoginUrlWithoutTokenTriggersLoginFinished() {
        registerFakeApp(IDP_PACKAGE, "application/web-identity+json", LOGIN_URL);

        doAnswer(
                        invocation -> {
                            IntentCallback callback = invocation.getArgument(1);
                            callback.onIntentCompleted(Activity.RESULT_OK, new Intent());
                            return true;
                        })
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(IntentCallback.class), any());

        mCoordinator.showModalDialog(LOGIN_URL);

        verify(mMockDelegate, never()).onNativeAppResult(any(String.class));
        verify(mMockDelegate).onNativeAppLoginFinished();
        verify(mMockDelegate, never()).onDismissed(any(Integer.class));
    }

    @Test
    public void testSuccessfulNativeFlow() {
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
                            resultIntent.putExtra(
                                    AccountSelectionCoordinator.EXTRA_TOKEN, "success_token");
                            callback.onIntentCompleted(Activity.RESULT_OK, resultIntent);
                            return true;
                        })
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(IntentCallback.class), any());

        mCoordinator.showModalDialog(CONTINUE_URL);

        // Verify that native app result is propagated
        verify(mMockDelegate).onNativeAppResult("success_token");
        verify(mMockDelegate, never()).onDismissed(any(Integer.class));
        assertNull(mShadowActivity.getNextStartedActivity());
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
                            listener.onOriginVerified(IDP_PACKAGE, origin, false, TriState.TRUE);
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
                            listener.onOriginVerified(pkg1, origin, false, TriState.TRUE);
                            return null;
                        })
                .doAnswer(
                        invocation -> {
                            OriginVerificationListener listener = invocation.getArgument(0);
                            Origin origin = invocation.getArgument(1);
                            listener.onOriginVerified(pkg2, origin, true, TriState.TRUE);
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

    private void registerFakeAppForDelegatedFlow(String packageName, GURL configUrl) {
        Intent intent = new Intent(AccountSelectionCoordinator.ACTION_ACTIVE_MODE_VIEW);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        intent.setData(Uri.parse(configUrl.getSpec()));
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.packageName = packageName;
        resolveInfo.activityInfo.name = packageName + ".DelegatedFlowActivity";
        mShadowPackageManager.addResolveInfoForIntent(intent, resolveInfo);
    }

    private NativeAppRequestOptions createTestRequestOptions(GURL configUrl) {
        return new NativeAppRequestOptions(
                configUrl, "https://rp.com", ASSERTION_PARAMS, "hint@example.com", "domain.com");
    }

    @Test
    public void testSuccessfulDelegatedFlowWithToken() {
        GURL configUrl = new GURL("https://idp.com/fedcm.json");
        registerFakeAppForDelegatedFlow(IDP_PACKAGE, configUrl);

        doAnswer(
                        invocation -> {
                            Intent intent = invocation.getArgument(0);
                            IntentCallback callback = invocation.getArgument(1);

                            assertEquals(IDP_PACKAGE, intent.getPackage());
                            assertEquals(
                                    AccountSelectionCoordinator.ACTION_ACTIVE_MODE_VIEW,
                                    intent.getAction());
                            assertEquals(configUrl.getSpec(), intent.getDataString());
                            assertEquals(
                                    "https://rp.com",
                                    intent.getStringExtra(
                                            AccountSelectionCoordinator.EXTRA_RP_ORIGIN));
                            assertEquals(
                                    ASSERTION_PARAMS,
                                    intent.getStringExtra(
                                            AccountSelectionCoordinator.EXTRA_ASSERTION_PARAMS));
                            assertEquals(
                                    "hint@example.com",
                                    intent.getStringExtra(
                                            AccountSelectionCoordinator.EXTRA_LOGIN_HINT));
                            assertEquals(
                                    "domain.com",
                                    intent.getStringExtra(
                                            AccountSelectionCoordinator.EXTRA_DOMAIN_HINT));

                            Intent resultIntent = new Intent();
                            resultIntent.putExtra(
                                    AccountSelectionCoordinator.EXTRA_TOKEN, "delegated_token");
                            callback.onIntentCompleted(Activity.RESULT_OK, resultIntent);
                            return true;
                        })
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(IntentCallback.class), any());

        boolean result = mCoordinator.showNativeAppUi(createTestRequestOptions(configUrl));
        assertEquals(true, result);

        verify(mMockDelegate).onNativeAppResult("delegated_token");
        verify(mMockDelegate, never()).onDismissed(anyInt());
    }

    // Launching a native app is a UI surface, so it must be suppressed while UI
    // is not allowed (e.g. an AI agent is driving the tab).
    @Test
    public void testDelegatedFlowSuppressedWhenUiNotAllowed() {
        GURL configUrl = new GURL("https://idp.com/fedcm.json");
        registerFakeAppForDelegatedFlow(IDP_PACKAGE, configUrl);

        AccountSelectionCoordinator coordinator =
                new AccountSelectionCoordinator(
                        mTab,
                        mWindowAndroid,
                        mBottomSheetController,
                        0, // RpMode.PASSIVE
                        false, // canShowUi
                        mMockDelegate);

        assertEquals(false, coordinator.showNativeAppUi(createTestRequestOptions(configUrl)));
        verify(mWindowAndroid, never())
                .showIntent(any(Intent.class), any(IntentCallback.class), any());
    }

    // Digital Asset Links verification is asynchronous. If the request is torn
    // down while it is in flight, the late callback must not put an app in
    // front of the user for a request that no longer exists.
    @Test
    public void testDelegatedFlowDoesNotLaunchAfterDismissal() {
        GURL configUrl = new GURL("https://idp.com/fedcm.json");
        registerFakeAppForDelegatedFlow(IDP_PACKAGE, configUrl);

        // Hold the verification result so we control exactly when it lands.
        List<Runnable> pendingVerifications = new ArrayList<>();
        doAnswer(
                        invocation -> {
                            OriginVerificationListener listener = invocation.getArgument(0);
                            Origin origin = invocation.getArgument(1);
                            pendingVerifications.add(
                                    () ->
                                            listener.onOriginVerified(
                                                    IDP_PACKAGE, origin, true, TriState.TRUE));
                            return null;
                        })
                .when(mMockOriginVerifier)
                .start(any(OriginVerificationListener.class), any(Origin.class));

        assertEquals(true, mCoordinator.showNativeAppUi(createTestRequestOptions(configUrl)));
        assertEquals(1, pendingVerifications.size());

        mCoordinator.close();
        pendingVerifications.get(0).run();

        verify(mWindowAndroid, never())
                .showIntent(any(Intent.class), any(IntentCallback.class), any());
    }

    // The hints are optional, so they must be omitted entirely rather
    // than passed as empty values that the app would have to special-case.
    @Test
    public void testDelegatedFlowOmitsEmptyOptionalExtras() {
        GURL configUrl = new GURL("https://idp.com/fedcm.json");
        registerFakeAppForDelegatedFlow(IDP_PACKAGE, configUrl);

        doAnswer(
                        invocation -> {
                            Intent intent = invocation.getArgument(0);
                            assertEquals(
                                    ASSERTION_PARAMS,
                                    intent.getStringExtra(
                                            AccountSelectionCoordinator.EXTRA_ASSERTION_PARAMS));
                            assertNull(
                                    intent.getStringExtra(
                                            AccountSelectionCoordinator.EXTRA_LOGIN_HINT));
                            assertNull(
                                    intent.getStringExtra(
                                            AccountSelectionCoordinator.EXTRA_DOMAIN_HINT));
                            return true;
                        })
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(IntentCallback.class), any());

        NativeAppRequestOptions requestOptions =
                new NativeAppRequestOptions(configUrl, "https://rp.com", ASSERTION_PARAMS, "", "");
        assertEquals(true, mCoordinator.showNativeAppUi(requestOptions));
        verify(mWindowAndroid).showIntent(any(Intent.class), any(IntentCallback.class), any());
    }

    // Tests the FedCM Error API flow where the native IdP returns both an error_code
    // and an error_url to provide details about the failure to the RP.
    @Test
    public void testDelegatedFlowWithError() {
        GURL configUrl = new GURL("https://idp.com/fedcm.json");
        registerFakeAppForDelegatedFlow(IDP_PACKAGE, configUrl);

        doAnswer(
                        invocation -> {
                            IntentCallback callback = invocation.getArgument(1);
                            Intent resultIntent = new Intent();
                            resultIntent.putExtra(
                                    AccountSelectionCoordinator.EXTRA_ERROR_CODE, "access_denied");
                            resultIntent.putExtra(
                                    AccountSelectionCoordinator.EXTRA_ERROR_URL,
                                    "https://idp.com/error");
                            callback.onIntentCompleted(Activity.RESULT_OK, resultIntent);
                            return true;
                        })
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(IntentCallback.class), any());

        boolean result = mCoordinator.showNativeAppUi(createTestRequestOptions(configUrl));
        assertEquals(true, result);

        ArgumentCaptor<IdentityCredentialTokenError> errorCaptor =
                ArgumentCaptor.forClass(IdentityCredentialTokenError.class);
        verify(mMockDelegate).onNativeAppError(errorCaptor.capture());
        assertEquals("access_denied", errorCaptor.getValue().getCode());
        assertEquals("https://idp.com/error", errorCaptor.getValue().getUrl().getSpec());
    }

    // Tests the FedCM Error API flow where the native IdP returns an error_code without an
    // optional error_url, verifying that an empty GURL is safely constructed.
    @Test
    public void testDelegatedFlowWithErrorNullUrl() {
        GURL configUrl = new GURL("https://idp.com/fedcm.json");
        registerFakeAppForDelegatedFlow(IDP_PACKAGE, configUrl);

        doAnswer(
                        invocation -> {
                            IntentCallback callback = invocation.getArgument(1);
                            Intent resultIntent = new Intent();
                            resultIntent.putExtra(
                                    AccountSelectionCoordinator.EXTRA_ERROR_CODE, "access_denied");
                            callback.onIntentCompleted(Activity.RESULT_OK, resultIntent);
                            return true;
                        })
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(IntentCallback.class), any());

        boolean result = mCoordinator.showNativeAppUi(createTestRequestOptions(configUrl));
        assertEquals(true, result);

        ArgumentCaptor<IdentityCredentialTokenError> errorCaptor =
                ArgumentCaptor.forClass(IdentityCredentialTokenError.class);
        verify(mMockDelegate).onNativeAppError(errorCaptor.capture());
        assertEquals("access_denied", errorCaptor.getValue().getCode());
        assertEquals("", errorCaptor.getValue().getUrl().getSpec());
    }

    @Test
    public void testDelegatedFlowDismissedWhenLaunchFails() {
        GURL configUrl = new GURL("https://idp.com/fedcm.json");
        registerFakeAppForDelegatedFlow(IDP_PACKAGE, configUrl);

        when(mWindowAndroid.showIntent(any(Intent.class), any(IntentCallback.class), any()))
                .thenReturn(false);

        boolean result = mCoordinator.showNativeAppUi(createTestRequestOptions(configUrl));
        assertEquals(true, result);

        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        verify(mMockDelegate, never()).onNativeAppResult(any());
    }

    @Test
    public void testDelegatedFlowDismissedWhenActivityCanceled() {
        GURL configUrl = new GURL("https://idp.com/fedcm.json");
        registerFakeAppForDelegatedFlow(IDP_PACKAGE, configUrl);

        doAnswer(
                        invocation -> {
                            IntentCallback callback = invocation.getArgument(1);
                            callback.onIntentCompleted(Activity.RESULT_CANCELED, null);
                            return true;
                        })
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(IntentCallback.class), any());

        boolean result = mCoordinator.showNativeAppUi(createTestRequestOptions(configUrl));
        assertEquals(true, result);

        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        verify(mMockDelegate, never()).onNativeAppResult(any());
    }

    @Test
    public void testDelegatedFlowTriesNextAppWhenFirstFailsVerification() {
        GURL configUrl = new GURL("https://idp.com/fedcm.json");
        String pkg1 = "com.idp.app1";
        String pkg2 = "com.idp.app2";
        registerFakeAppForDelegatedFlow(pkg1, configUrl);
        registerFakeAppForDelegatedFlow(pkg2, configUrl);

        // App1 fails verification, App2 succeeds verification
        doAnswer(
                        invocation -> {
                            OriginVerificationListener listener = invocation.getArgument(0);
                            Origin origin = invocation.getArgument(1);
                            listener.onOriginVerified(pkg1, origin, false, TriState.TRUE);
                            return null;
                        })
                .doAnswer(
                        invocation -> {
                            OriginVerificationListener listener = invocation.getArgument(0);
                            Origin origin = invocation.getArgument(1);
                            listener.onOriginVerified(pkg2, origin, true, TriState.TRUE);
                            return null;
                        })
                .when(mMockOriginVerifier)
                .start(any(OriginVerificationListener.class), any(Origin.class));

        doAnswer(
                        invocation -> {
                            Intent intent = invocation.getArgument(0);
                            IntentCallback callback = invocation.getArgument(1);
                            assertEquals(pkg2, intent.getPackage());
                            Intent resultIntent = new Intent();
                            resultIntent.putExtra(
                                    AccountSelectionCoordinator.EXTRA_TOKEN,
                                    "delegated_token_app2");
                            callback.onIntentCompleted(Activity.RESULT_OK, resultIntent);
                            return true;
                        })
                .when(mWindowAndroid)
                .showIntent(any(Intent.class), any(IntentCallback.class), any());

        boolean result = mCoordinator.showNativeAppUi(createTestRequestOptions(configUrl));
        assertEquals(true, result);

        verify(mMockDelegate).onNativeAppResult("delegated_token_app2");
        verify(mMockDelegate, never()).onDismissed(anyInt());
    }

    @Test
    public void testDelegatedFlowDismissedWhenNoAppInstalled() {
        GURL configUrl = new GURL("https://idp.com/fedcm.json");

        boolean result = mCoordinator.showNativeAppUi(createTestRequestOptions(configUrl));
        assertEquals(true, result);

        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        verify(mMockDelegate, never()).onNativeAppResult(any());
        verify(mWindowAndroid, never()).showIntent(any(Intent.class), any(), any());
    }

    @Test
    public void testDelegatedFlowDismissedWhenVerificationFails() {
        GURL configUrl = new GURL("https://idp.com/fedcm.json");
        registerFakeAppForDelegatedFlow(IDP_PACKAGE, configUrl);

        // Mock verification failure
        doAnswer(
                        invocation -> {
                            OriginVerificationListener listener = invocation.getArgument(0);
                            Origin origin = invocation.getArgument(1);
                            listener.onOriginVerified(IDP_PACKAGE, origin, false, TriState.TRUE);
                            return null;
                        })
                .when(mMockOriginVerifier)
                .start(any(OriginVerificationListener.class), any(Origin.class));

        boolean result = mCoordinator.showNativeAppUi(createTestRequestOptions(configUrl));
        assertEquals(true, result);

        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        verify(mMockDelegate, never()).onNativeAppResult(any());
        verify(mWindowAndroid, never()).showIntent(any(Intent.class), any(), any());
    }

    @Test
    @DisableFeatures(ContentFeatures.FED_CM_NATIVE_ID_PS)
    public void testDelegatedFlowReturnsFalseWhenFeatureDisabled() {
        GURL configUrl = new GURL("https://idp.com/fedcm.json");
        registerFakeAppForDelegatedFlow(IDP_PACKAGE, configUrl);

        boolean result = mCoordinator.showNativeAppUi(createTestRequestOptions(configUrl));
        assertEquals(false, result);

        verify(mMockDelegate, never()).onDismissed(anyInt());
        verify(mMockDelegate, never()).onNativeAppResult(any());
    }
}
