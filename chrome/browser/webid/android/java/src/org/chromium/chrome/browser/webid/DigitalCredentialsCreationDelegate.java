// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import static androidx.core.app.ActivityCompat.startIntentSenderForResult;

import android.app.Activity;
import android.content.Intent;
import android.content.IntentSender.SendIntentException;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ResultReceiver;

import androidx.annotation.OptIn;
import androidx.credentials.DigitalCredential;
import androidx.credentials.exceptions.CreateCredentialException;
import androidx.credentials.exceptions.CreateCredentialUnknownException;
import androidx.credentials.provider.PendingIntentHandler;

import com.google.android.gms.identitycredentials.CreateCredentialRequest;
import com.google.android.gms.identitycredentials.IdentityCredentialClient;
import com.google.android.gms.identitycredentials.IdentityCredentialManager;

import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.build.annotations.NullMarked;

@NullMarked
public class DigitalCredentialsCreationDelegate {
    private static final String TAG = "DCCreationDelegate";

    // Arbitrary request code that is used when invoking the GMSCore API.
    private static final int REQUEST_CODE_DIGITAL_CREDENTIALS_CREATION = 777;

    private static final String BUNDLE_KEY_PROVIDER_DATA = "androidx.identitycredentials.BUNDLE_KEY_PROVIDER_DATA";

    @OptIn(markerClass = androidx.credentials.ExperimentalDigitalCredentialApi.class)
    public Promise<String> create(Activity window, String origin, String request) {
        final IdentityCredentialClient client =
                IdentityCredentialManager.Companion.getClient(window);

        final Promise<String> result = new Promise<String>();

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
                                Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                                        ? data.getParcelable(
                                                BUNDLE_KEY_PROVIDER_DATA, Intent.class)
                                        : data.getParcelable(BUNDLE_KEY_PROVIDER_DATA);

                        if (providerData == null) {
                            Log.d(TAG, "Response doesn't contain providerData");
                            result.reject(
                                    new CreateCredentialUnknownException(
                                            "Response doesn't contain providerData"));
                            return;
                        }

                        var response =
                                PendingIntentHandler.retrieveCreateCredentialResponse(
                                        DigitalCredential.TYPE_DIGITAL_CREDENTIAL, providerData);
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
                        result.fulfill(responseJson);
                    }
                };

        Bundle requestBundle = new Bundle();
        requestBundle.putString("androidx.credentials.BUNDLE_KEY_REQUEST_JSON", request);
        CreateCredentialRequest createRequest =
                new CreateCredentialRequest(
                        DigitalCredential.TYPE_DIGITAL_CREDENTIAL,
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
                            try {
                                Log.d(TAG, "Sending an intent for sender");
                                Log.d(TAG, request);
                                startIntentSenderForResult(
                                        /* activity= */ window,
                                        /* intent= */ response.getPendingIntent().getIntentSender(),
                                        /* requestCode= */ REQUEST_CODE_DIGITAL_CREDENTIALS_CREATION,
                                        /* fillInIntent= */ null,
                                        /* flagsMask= */ 0,
                                        /* flagsValues= */ 0,
                                        /* extraFlags= */ 0,
                                        /* options= */ null);
                            } catch (SendIntentException e) {
                                Log.e(TAG, "Sending an intent for sender failed");
                                if (result.isPending()) {
                                    result.reject(e);
                                }
                            }
                        })
                .addOnFailureListener(
                        e -> {
                            result.reject(new CreateCredentialUnknownException("unknown error"));
                        });

        return result;
    }
}
