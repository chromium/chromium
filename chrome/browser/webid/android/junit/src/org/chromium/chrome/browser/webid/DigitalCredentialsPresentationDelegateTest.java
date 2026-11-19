// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Looper;

import androidx.credentials.GetCredentialResponse;
import androidx.credentials.GetDigitalCredentialOption;
import androidx.credentials.exceptions.GetCredentialException;
import androidx.credentials.exceptions.GetCredentialUnknownException;
import androidx.credentials.provider.PendingIntentHandler;

import com.google.android.gms.identitycredentials.GetCredentialRequest;
import com.google.android.gms.identitycredentials.IdentityCredentialClient;
import com.google.android.gms.identitycredentials.PendingGetCredentialHandle;
import com.google.android.gms.tasks.TaskCompletionSource;

import org.json.JSONException;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.webid.IdentityCredentialsDelegate.DigitalCredential;
import org.chromium.content_public.browser.webid.DigitalIdentityUiWarningLevel;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link DigitalCredentialsPresentationDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = {Build.VERSION_CODES.TIRAMISU, Build.VERSION_CODES.UPSIDE_DOWN_CAKE})
public class DigitalCredentialsPresentationDelegateTest {

    private static final String JSON_PROTOCOL = "openid4vp";
    private static final String JSON_DATA = "{\"test_key\":\"test_value\"}";
    private static final String JSON_WITH_PROTOCOL =
            "{\"protocol\" : \"openid4vp\", \"data\": {\"test_key\":\"test_value\"}}";
    private static final String JSON_WITHOUT_PROTOCOL = "{\"data\": {\"test_key\":\"test_value\"}}";

    private void packageResponseJson(String json, Intent intent) {
        PendingIntentHandler.setGetCredentialResponse(
                intent,
                new GetCredentialResponse(new androidx.credentials.DigitalCredential(json)));
    }

    @Test
    public void testExtractDigitalCredentialFromGetResponse()
            throws GetCredentialException, JSONException {
        Intent intent = new Intent();
        packageResponseJson(JSON_WITH_PROTOCOL, intent);

        DigitalCredential extractedCredential =
                DigitalCredentialsPresentationDelegate.extractDigitalCredentialFromIntent(intent);

        assertNotNull(extractedCredential);
        assertEquals(JSON_PROTOCOL, extractedCredential.mProtocol);
        assertEquals(JSON_DATA, extractedCredential.mData);
    }

    @Test
    public void testExtractDigitalCredentialFromGetResponse_NoProtocol()
            throws GetCredentialException, JSONException {
        Intent intent = new Intent();
        packageResponseJson(JSON_WITHOUT_PROTOCOL, intent);

        assertThrows(
                JSONException.class,
                () ->
                        DigitalCredentialsPresentationDelegate.extractDigitalCredentialFromIntent(
                                intent));
    }

    @Test
    public void testExtractDigitalCredentialFromGetResponse_Exception() {
        Intent intent = new Intent();
        PendingIntentHandler.setGetCredentialException(
                intent, new GetCredentialUnknownException("test error"));

        assertThrows(
                GetCredentialException.class,
                () ->
                        DigitalCredentialsPresentationDelegate.extractDigitalCredentialFromIntent(
                                intent));
    }

    @Test
    public void testHandleOnReceiveResult_LargePayload()
            throws GetCredentialException, JSONException {
        DigitalCredentialsPresentationDelegate delegate =
                new DigitalCredentialsPresentationDelegate();
        Promise<DigitalCredential> promise = new Promise<>();

        Bundle data = new Bundle();
        Intent providerData = new Intent();
        providerData.putExtra(
                DigitalCredentialsPresentationDelegate.EXTRA_PASS_IT_BY_RESULT_RECEIVER, true);
        data.putParcelable(
                DigitalCredentialsPresentationDelegate.BUNDLE_KEY_PROVIDER_DATA, providerData);

        Bundle largeResultData = new Bundle();
        Intent realProviderData = new Intent();
        packageResponseJson(JSON_WITH_PROTOCOL, realProviderData);
        largeResultData.putParcelable(
                DigitalCredentialsPresentationDelegate.RESULT_DATA, realProviderData);

        delegate.handleOnReceiveResult(Activity.RESULT_OK, data, promise, largeResultData);

        assertTrue(promise.isFulfilled());
        DigitalCredential credential = promise.getResult();
        assertNotNull(credential);
        assertEquals(JSON_PROTOCOL, credential.mProtocol);
        assertEquals(JSON_DATA, credential.mData);
    }

    @Test
    public void testGetWithDeadActivityReturnsRejected() {
        WindowAndroid windowAndroid = mock(WindowAndroid.class);
        when(windowAndroid.getActivity()).thenReturn(new ImmutableWeakReference<>(null));

        DigitalCredentialsPresentationDelegate delegate =
                new DigitalCredentialsPresentationDelegate();
        Promise<DigitalCredential> promise =
                delegate.get(
                        windowAndroid,
                        "https://rp.example",
                        "{\"requests\":[]}",
                        DigitalIdentityUiWarningLevel.HINT_CAUTION);

        assertTrue(promise.isRejected());
    }

    @Test
    public void testGetWithDestroyedWindowReturnsRejected() {
        WindowAndroid windowAndroid = mock(WindowAndroid.class);
        when(windowAndroid.isDestroyed()).thenReturn(true);

        DigitalCredentialsPresentationDelegate delegate =
                new DigitalCredentialsPresentationDelegate();
        Promise<DigitalCredential> promise =
                delegate.get(
                        windowAndroid,
                        "https://rp.example",
                        "{\"requests\":[]}",
                        DigitalIdentityUiWarningLevel.HINT_CAUTION);

        assertTrue(promise.isRejected());
    }

    @Test
    public void testGetWhenWindowDestroyedInFlightRejectsAndSkipsIntent() {
        Activity activity = Robolectric.buildActivity(Activity.class).get();
        WindowAndroid windowAndroid = mock(WindowAndroid.class);
        when(windowAndroid.getActivity()).thenReturn(new ImmutableWeakReference<>(activity));
        when(windowAndroid.isDestroyed()).thenReturn(false);

        IdentityCredentialClient mockClient = mock(IdentityCredentialClient.class);
        TaskCompletionSource<PendingGetCredentialHandle> taskSource = new TaskCompletionSource<>();
        when(mockClient.getCredential(any())).thenReturn(taskSource.getTask());

        DigitalCredentialsPresentationDelegate delegate =
                new DigitalCredentialsPresentationDelegate() {
                    @Override
                    IdentityCredentialClient getIdentityCredentialClient(Activity window) {
                        return mockClient;
                    }
                };

        Promise<DigitalCredential> promise =
                delegate.get(
                        windowAndroid,
                        "https://rp.example",
                        "{\"requests\":[]}",
                        DigitalIdentityUiWarningLevel.HINT_NO_ISSUES);
        assertTrue(promise.isPending());

        when(windowAndroid.isDestroyed()).thenReturn(true);
        PendingIntent pendingIntent =
                PendingIntent.getActivity(activity, 0, new Intent(), PendingIntent.FLAG_IMMUTABLE);
        taskSource.setResult(new PendingGetCredentialHandle(pendingIntent));
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        assertTrue(promise.isRejected());
        verify(windowAndroid, never()).showCancelableIntent(any(PendingIntent.class), any(), any());
    }

    @Test
    public void testGetMapsWarningLevelHint() {
        Activity activity = Robolectric.buildActivity(Activity.class).get();
        WindowAndroid windowAndroid = mock(WindowAndroid.class);
        when(windowAndroid.getActivity()).thenReturn(new ImmutableWeakReference<>(activity));

        IdentityCredentialClient mockClient = mock(IdentityCredentialClient.class);
        ArgumentCaptor<GetCredentialRequest> requestCaptor =
                ArgumentCaptor.forClass(GetCredentialRequest.class);
        when(mockClient.getCredential(requestCaptor.capture()))
                .thenReturn(new TaskCompletionSource<PendingGetCredentialHandle>().getTask());

        DigitalCredentialsPresentationDelegate delegate =
                new DigitalCredentialsPresentationDelegate() {
                    @Override
                    IdentityCredentialClient getIdentityCredentialClient(Activity window) {
                        return mockClient;
                    }
                };

        delegate.get(
                windowAndroid,
                "https://rp.example",
                "{\"requests\":[]}",
                DigitalIdentityUiWarningLevel.HINT_NO_ISSUES);
        delegate.get(
                windowAndroid,
                "https://rp.example",
                "{\"requests\":[]}",
                DigitalIdentityUiWarningLevel.HINT_CAUTION);

        Bundle sentRequestData1 =
                requestCaptor
                        .getAllValues()
                        .get(0)
                        .getCredentialOptions()
                        .get(0)
                        .getCredentialRetrievalData();
        assertEquals(
                GetDigitalCredentialOption.UI_WARNING_LEVEL_HINT_NO_ISSUES,
                sentRequestData1.getInt(
                        GetDigitalCredentialOption.BUNDLE_KEY_UI_WARNING_LEVEL_HINT, -1));

        Bundle sentRequestData2 =
                requestCaptor
                        .getAllValues()
                        .get(1)
                        .getCredentialOptions()
                        .get(0)
                        .getCredentialRetrievalData();
        assertEquals(
                GetDigitalCredentialOption.UI_WARNING_LEVEL_HINT_CAUTION,
                sentRequestData2.getInt(
                        GetDigitalCredentialOption.BUNDLE_KEY_UI_WARNING_LEVEL_HINT, -1));
    }

    @Test
    public void testCancelDismissesInFlightIntentAndSuppressesResult() {
        Activity activity = Robolectric.buildActivity(Activity.class).get();
        WindowAndroid windowAndroid = mock(WindowAndroid.class);
        when(windowAndroid.getActivity()).thenReturn(new ImmutableWeakReference<>(activity));
        when(windowAndroid.isDestroyed()).thenReturn(false);
        // Use request code `0` to verify `START_INTENT_FAILURE` (-1) is used as the
        // inactive sentinel rather than `0`.
        when(windowAndroid.showCancelableIntent(any(PendingIntent.class), any(), any()))
                .thenReturn(0);

        IdentityCredentialClient mockClient = mock(IdentityCredentialClient.class);
        TaskCompletionSource<PendingGetCredentialHandle> taskSource = new TaskCompletionSource<>();
        when(mockClient.getCredential(any())).thenReturn(taskSource.getTask());

        DigitalCredentialsPresentationDelegate delegate =
                new DigitalCredentialsPresentationDelegate() {
                    @Override
                    IdentityCredentialClient getIdentityCredentialClient(Activity window) {
                        return mockClient;
                    }
                };

        Promise<DigitalCredential> promise =
                delegate.get(
                        windowAndroid,
                        "https://rp.example",
                        "{\"requests\":[]}",
                        DigitalIdentityUiWarningLevel.HINT_NO_ISSUES);
        PendingIntent pendingIntent =
                PendingIntent.getActivity(activity, 0, new Intent(), PendingIntent.FLAG_IMMUTABLE);
        taskSource.setResult(new PendingGetCredentialHandle(pendingIntent));
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        assertTrue(promise.isPending());
        delegate.cancel();
        verify(windowAndroid).cancelIntent(0);

        // Any late `ResultReceiver` callback arriving after `cancel()` must reject the
        // promise rather than fulfilling a credential.
        Bundle data = new Bundle();
        Intent providerData = new Intent();
        packageResponseJson(JSON_WITH_PROTOCOL, providerData);
        data.putParcelable(
                DigitalCredentialsPresentationDelegate.BUNDLE_KEY_PROVIDER_DATA, providerData);
        delegate.handleOnReceiveResult(Activity.RESULT_OK, data, promise, Bundle.EMPTY);
        assertTrue(promise.isRejected());
    }

    @Test
    public void testCancelWhileTaskPendingSkipsIntentAndRejects() {
        Activity activity = Robolectric.buildActivity(Activity.class).get();
        WindowAndroid windowAndroid = mock(WindowAndroid.class);
        when(windowAndroid.getActivity()).thenReturn(new ImmutableWeakReference<>(activity));
        when(windowAndroid.isDestroyed()).thenReturn(false);

        IdentityCredentialClient mockClient = mock(IdentityCredentialClient.class);
        TaskCompletionSource<PendingGetCredentialHandle> taskSource = new TaskCompletionSource<>();
        when(mockClient.getCredential(any())).thenReturn(taskSource.getTask());

        DigitalCredentialsPresentationDelegate delegate =
                new DigitalCredentialsPresentationDelegate() {
                    @Override
                    IdentityCredentialClient getIdentityCredentialClient(Activity window) {
                        return mockClient;
                    }
                };

        Promise<DigitalCredential> promise =
                delegate.get(
                        windowAndroid,
                        "https://rp.example",
                        "{\"requests\":[]}",
                        DigitalIdentityUiWarningLevel.HINT_NO_ISSUES);
        assertTrue(promise.isPending());

        delegate.cancel();

        PendingIntent pendingIntent =
                PendingIntent.getActivity(activity, 0, new Intent(), PendingIntent.FLAG_IMMUTABLE);
        taskSource.setResult(new PendingGetCredentialHandle(pendingIntent));
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        assertTrue(promise.isRejected());
        verify(windowAndroid, never()).showCancelableIntent(any(PendingIntent.class), any(), any());
    }
}
