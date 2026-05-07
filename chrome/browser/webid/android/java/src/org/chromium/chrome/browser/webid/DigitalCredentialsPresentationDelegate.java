// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ResultReceiver;

import androidx.annotation.OptIn;
import androidx.annotation.VisibleForTesting;
import androidx.credentials.Credential;
import androidx.credentials.ExperimentalDigitalCredentialApi;
import androidx.credentials.GetDigitalCredentialOption;
import androidx.credentials.exceptions.GetCredentialCancellationException;
import androidx.credentials.exceptions.GetCredentialInterruptedException;
import androidx.credentials.exceptions.GetCredentialUnknownException;
import androidx.credentials.exceptions.NoCredentialException;
import androidx.credentials.provider.PendingIntentHandler;

import com.google.android.gms.identitycredentials.CredentialOption;
import com.google.android.gms.identitycredentials.GetCredentialRequest;
import com.google.android.gms.identitycredentials.IdentityCredentialClient;
import com.google.android.gms.identitycredentials.IdentityCredentialManager;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.webid.IdentityCredentialsDelegate.DigitalCredential;
import org.chromium.ui.base.WindowAndroid;

import java.util.Arrays;

@NullMarked
public class DigitalCredentialsPresentationDelegate {
    private static final String TAG = "DCPresentDelegate";

    @VisibleForTesting public static final String DC_API_RESPONSE_PROTOCOL_KEY = "protocol";
    @VisibleForTesting public static final String DC_API_RESPONSE_DATA_KEY = "data";

    @VisibleForTesting
    public static final String BUNDLE_KEY_PROVIDER_DATA =
            "androidx.identitycredentials.BUNDLE_KEY_PROVIDER_DATA";

    private static final String EXTRA_LARGE_PAYLOAD_RESULT_RECEIVER =
            "androidx.credentials.provider.EXTRA_LARGE_PAYLOAD_RESULT_RECEIVER";

    @VisibleForTesting
    public static final String EXTRA_PASS_IT_BY_RESULT_RECEIVER =
            "androidx.credentials.provider.EXTRA_PASS_IT_BY_RESULT_RECEIVER";

    @VisibleForTesting public static final String RESULT_DATA = "RESULT_DATA";

    private static final String TYPE_USER_CANCELED =
            "android.credentials.GetCredentialException.TYPE_USER_CANCELED";
    private static final String TYPE_NO_CREDENTIAL =
            "android.credentials.GetCredentialException.TYPE_NO_CREDENTIAL";
    private static final String TYPE_INTERRUPTED =
            "android.credentials.GetCredentialException.TYPE_INTERRUPTED";

    private Bundle mLargeResultData = Bundle.EMPTY;

    @OptIn(markerClass = ExperimentalDigitalCredentialApi.class)
    public Promise<DigitalCredential> get(
            WindowAndroid windowAndroid, String origin, String request) {
        Activity window = windowAndroid.getActivity().get();
        if (window == null) return Promise.rejected();

        final IdentityCredentialClient client;
        try {
            client = IdentityCredentialManager.Companion.getClient(window);
        } catch (Exception e) {
            // Thrown when running in phones without the most current GMS
            // version.
            return Promise.rejected();
        }

        final Promise<DigitalCredential> result = new Promise<>();

        final ResultReceiver largePayloadResultReceiver =
                new ResultReceiver(new Handler(Looper.getMainLooper())) {
                    @Override
                    protected void onReceiveResult(int resultCode, Bundle resultData) {
                        mLargeResultData = resultData;
                    }
                };

        ResultReceiver resultReceiver =
                new ResultReceiver(new Handler(Looper.getMainLooper())) {
                    @Override
                    protected void onReceiveResult(int code, Bundle data) {
                        try {
                            handleOnReceiveResult(code, data, result, mLargeResultData);
                        } finally {
                            mLargeResultData = Bundle.EMPTY; // release the reference
                        }
                    }
                };

        GetDigitalCredentialOption option = new GetDigitalCredentialOption(request);
        Bundle requestData = option.getRequestData();
        requestData.putParcelable(
                EXTRA_LARGE_PAYLOAD_RESULT_RECEIVER,
                toIpcFriendlyResultReceiver(largePayloadResultReceiver));

        client.getCredential(
                        new GetCredentialRequest(
                                Arrays.asList(
                                        new CredentialOption(
                                                option.getType(),
                                                requestData,
                                                option.getCandidateQueryData(),
                                                request,
                                                "",
                                                "")),
                                new Bundle(),
                                origin,
                                resultReceiver))
                .addOnSuccessListener(
                        response -> {
                            if (response.getPendingIntent() == null) {
                                Log.d(TAG, "Response doesn't contain pendingIntent");
                                result.reject(
                                        new GetCredentialUnknownException(
                                                "Response doesn't contain pendingIntent"));
                                return;
                            }
                            Log.d(TAG, "Sending an intent for sender");
                            Log.d(TAG, request);
                            int requestCode =
                                    windowAndroid.showCancelableIntent(
                                            response.getPendingIntent(),
                                            (resultCode, intent) -> {
                                                if (resultCode != Activity.RESULT_OK
                                                        && result.isPending()) {
                                                    androidx.credentials.exceptions
                                                                    .GetCredentialException
                                                            exception =
                                                                    intent != null
                                                                            ? PendingIntentHandler
                                                                                    .retrieveGetCredentialException(
                                                                                            intent)
                                                                            : null;
                                                    handleGetCredentialException(
                                                            resultCode, exception, result);
                                                }
                                            },
                                            null);
                            if (requestCode == WindowAndroid.START_INTENT_FAILURE) {
                                Log.e(TAG, "Sending an intent for sender failed");
                                result.reject(
                                        new GetCredentialUnknownException(
                                                "Failed to start intent"));
                            }
                        })
                .addOnFailureListener(
                        e -> {
                            if (!result.isPending()) {
                                return;
                            }
                            if (e
                                    instanceof
                                    com.google.android.gms.identitycredentials
                                            .GetCredentialException) {
                                String exceptionType =
                                        ((com.google.android.gms.identitycredentials
                                                                .GetCredentialException)
                                                        e)
                                                .getType();
                                if (TYPE_USER_CANCELED.equals(exceptionType)) {
                                    result.reject(
                                            new GetCredentialCancellationException(e.getMessage()));
                                } else if (TYPE_NO_CREDENTIAL.equals(exceptionType)) {
                                    result.reject(new NoCredentialException(e.getMessage()));
                                } else if (TYPE_INTERRUPTED.equals(exceptionType)) {
                                    result.reject(
                                            new GetCredentialInterruptedException(e.getMessage()));
                                } else {
                                    result.reject(
                                            new GetCredentialUnknownException(e.getMessage()));
                                }
                            } else {
                                result.reject(e);
                            }
                        });

        return result;
    }

    private <T extends ResultReceiver> @Nullable ResultReceiver toIpcFriendlyResultReceiver(
            @Nullable T resultReceiver) {
        if (resultReceiver == null) {
            return null;
        }
        android.os.Parcel parcel = android.os.Parcel.obtain();
        try {
            resultReceiver.writeToParcel(parcel, 0);
            parcel.setDataPosition(0);
            return ResultReceiver.CREATOR.createFromParcel(parcel);
        } catch (Exception e) {
            return null;
        } finally {
            parcel.recycle();
        }
    }

    @VisibleForTesting
    public void handleOnReceiveResult(
            int code,
            @Nullable Bundle data,
            Promise<DigitalCredential> result,
            Bundle largeResultData) {
        if (!result.isPending()) {
            return;
        }
        Log.d(TAG, "Received a response");
        try {
            Intent providerData =
                    data == null
                            ? null
                            : IntentUtils.safeGetParcelable(data, BUNDLE_KEY_PROVIDER_DATA);

            if (providerData != null && providerData.hasExtra(EXTRA_PASS_IT_BY_RESULT_RECEIVER)) {
                providerData = IntentUtils.safeGetParcelable(largeResultData, RESULT_DATA);
            }

            if (code != Activity.RESULT_OK) {
                androidx.credentials.exceptions.GetCredentialException exception =
                        providerData != null
                                ? PendingIntentHandler.retrieveGetCredentialException(providerData)
                                : null;
                handleGetCredentialException(code, exception, result);
                return;
            }
            var credential = extractDigitalCredentialFromIntent(providerData);
            if (credential == null) {
                result.reject(
                        new GetCredentialUnknownException(
                                "Response does not contain a credential"));
            } else {
                result.fulfill(credential);
            }
        } catch (androidx.credentials.exceptions.GetCredentialException e) {
            handleGetCredentialException(code, e, result);
        } catch (Exception e) {
            Log.e(TAG, e.toString());
            result.reject(new GetCredentialUnknownException(e.getMessage()));
        }
    }

    /**
     * Extracts a DigitalCredential from an intent, or throws an exception if found.
     *
     * @param intent The intent containing the response data.
     * @return The extracted DigitalCredential.
     * @throws androidx.credentials.exceptions.GetCredentialException If an error is found in the
     *     response.
     * @throws JSONException If JSON parsing fails.
     */
    @VisibleForTesting
    public static @Nullable DigitalCredential extractDigitalCredentialFromIntent(
            @Nullable Intent intent)
            throws androidx.credentials.exceptions.GetCredentialException, JSONException {
        if (intent == null) {
            return null;
        }

        var response = PendingIntentHandler.retrieveGetCredentialResponse(intent);
        if (response == null) {
            androidx.credentials.exceptions.GetCredentialException exception =
                    PendingIntentHandler.retrieveGetCredentialException(intent);
            if (exception != null) {
                throw exception;
            }
            return null;
        }

        Credential c = response.getCredential();
        if (!(c instanceof androidx.credentials.DigitalCredential)) {
            return null;
        }
        String credentialJson = ((androidx.credentials.DigitalCredential) c).getCredentialJson();
        if (credentialJson == null) {
            return null;
        }
        JSONObject credential = new JSONObject(credentialJson);
        String protocol = credential.getString(DC_API_RESPONSE_PROTOCOL_KEY);
        var data = credential.getJSONObject(DC_API_RESPONSE_DATA_KEY);
        return new DigitalCredential(protocol, data.toString());
    }

    private static void handleGetCredentialException(
            int resultCode,
            androidx.credentials.exceptions.@Nullable GetCredentialException exception,
            Promise<DigitalCredential> result) {
        if (exception != null) {
            result.reject(exception);
        } else if (resultCode == Activity.RESULT_CANCELED) {
            result.reject(new GetCredentialCancellationException("Activity Canceled"));
        } else {
            Log.w(TAG, "Cannot process resultCode: " + resultCode);
            result.reject(
                    new GetCredentialUnknownException("Cannot process resultCode: " + resultCode));
        }
    }
}
