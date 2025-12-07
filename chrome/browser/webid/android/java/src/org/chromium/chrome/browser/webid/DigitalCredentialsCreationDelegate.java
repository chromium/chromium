// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import static androidx.credentials.DigitalCredential.TYPE_DIGITAL_CREDENTIAL;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ResultReceiver;

import androidx.annotation.OptIn;
import androidx.credentials.exceptions.CreateCredentialException;
import androidx.credentials.exceptions.CreateCredentialUnknownException;
import androidx.credentials.provider.PendingIntentHandler;

import com.google.android.gms.identitycredentials.CreateCredentialRequest;
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

@NullMarked
public class DigitalCredentialsCreationDelegate {
    private static final String TAG = "DCCreationDelegate";

    private static final String DC_API_RESPONSE_PROTOCOL_KEY = "protocol";
    private static final String DC_API_RESPONSE_DATA_KEY = "data";

    private static final String BUNDLE_KEY_PROVIDER_DATA =
            "androidx.identitycredentials.BUNDLE_KEY_PROVIDER_DATA";

    @OptIn(markerClass = androidx.credentials.ExperimentalDigitalCredentialApi.class)
    public Promise<DigitalCredential> create(
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

        ResultReceiver resultReceiver =
                new ResultReceiver(new Handler(Looper.getMainLooper())) {
                    @Override
                    protected void onReceiveResult(int code, Bundle data) {
                        if (!result.isPending()) {
                            // Promises don't support duplicate calls to fulfill/reject. If the
                            // promise has been fulfilled/rejected already (e.g. due to erroneous
                            // exception during the API call), return immediately.
                            return;
                        }
                        Log.d(TAG, "Received a response");
                        Intent providerData =
                                IntentUtils.safeGetParcelable(data, BUNDLE_KEY_PROVIDER_DATA);

                        if (providerData == null) {
                            Log.d(TAG, "Response doesn't contain providerData");
                            result.reject(
                                    new CreateCredentialUnknownException(
                                            "Response doesn't contain providerData"));
                            return;
                        }

                        var response =
                                PendingIntentHandler.retrieveCreateCredentialResponse(
                                        TYPE_DIGITAL_CREDENTIAL, providerData);
                        if (response == null) {
                            CreateCredentialException exception =
                                    PendingIntentHandler.retrieveCreateCredentialException(
                                            providerData);
                            result.reject(
                                    exception == null
                                            ? new CreateCredentialUnknownException("empty response")
                                            : exception);
                            return;
                        }
                        String responseJson =
                                response.getData()
                                        .getString("androidx.credentials.BUNDLE_KEY_RESPONSE_JSON");
                        if (responseJson == null) {
                            result.reject(
                                    new CreateCredentialUnknownException(
                                            "Response doesn't contain responseJson"));
                            return;
                        }
                        Log.d(TAG, "Response JSON: " + responseJson);

                        DigitalCredential digitalCredential = parseResponse(responseJson);
                        if (digitalCredential == null) {
                            result.reject(
                                    new CreateCredentialUnknownException(
                                            "Failed to parse response"));
                            return;
                        }
                        result.fulfill(digitalCredential);
                    }
                };

        Bundle requestBundle = new Bundle();
        requestBundle.putString("androidx.credentials.BUNDLE_KEY_REQUEST_JSON", request);
        CreateCredentialRequest createRequest =
                new CreateCredentialRequest(
                        TYPE_DIGITAL_CREDENTIAL,
                        /* credentialData= */ requestBundle,
                        /* candidateQueryData= */ new Bundle(),
                        origin,
                        request,
                        resultReceiver);

        client.createCredential(createRequest)
                .addOnSuccessListener(
                        response -> {
                            if (response.getPendingIntent() == null) {
                                Log.d(TAG, "Response doesn't contain pendingIntent");
                                result.reject(
                                        new CreateCredentialUnknownException(
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
                                                    result.reject(
                                                            new Exception("Cancelled or Crashed"));
                                                }
                                            },
                                            null);
                            if (requestCode == WindowAndroid.START_INTENT_FAILURE) {
                                Log.e(TAG, "Sending an intent for sender failed");
                                result.reject(new Exception("Failed to start intent"));
                            }
                        })
                .addOnFailureListener(
                        e -> {
                            result.reject(new CreateCredentialUnknownException("unknown error"));
                        });

        return result;
    }

    private static @Nullable DigitalCredential parseResponse(String responseJson) {
        try {
            JSONObject credentialJson = new JSONObject(responseJson);
            String protocol = credentialJson.getString(DC_API_RESPONSE_PROTOCOL_KEY);
            String data = credentialJson.getString(DC_API_RESPONSE_DATA_KEY);
            return new DigitalCredential(protocol, data);
        } catch (JSONException e) {
            Log.e(TAG, "Failed to parse response JSON: " + e);
            return null;
        }
    }
}
