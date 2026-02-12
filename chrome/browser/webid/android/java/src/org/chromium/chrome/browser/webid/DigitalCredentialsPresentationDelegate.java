// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ResultReceiver;

import androidx.annotation.OptIn;
import androidx.annotation.VisibleForTesting;
import androidx.credentials.Credential;
import androidx.credentials.GetDigitalCredentialOption;
import androidx.credentials.provider.PendingIntentHandler;

import com.google.android.gms.identitycredentials.CredentialOption;
import com.google.android.gms.identitycredentials.GetCredentialException;
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

    @OptIn(markerClass = androidx.credentials.ExperimentalDigitalCredentialApi.class)
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

        ResultReceiver resultReceiver =
                new ResultReceiver(new Handler(Looper.getMainLooper())) {
                    // android.credentials.GetCredentialException requires API level 34
                    @SuppressWarnings("NewApi")
                    @Override
                    protected void onReceiveResult(int code, Bundle data) {
                        Log.d(TAG, "Received a response");
                        try {
                            var credential = extractDigitalCredentialFromResponseBundle(code, data);
                            if (credential == null) {
                                result.reject(
                                        new Exception("Response does not contain a credential"));
                            } else {
                                result.fulfill(credential);
                            }
                        } catch (Exception e) {
                            Log.e(TAG, e.toString());

                            if (e instanceof GetCredentialException
                                    && Build.VERSION.SDK_INT
                                            >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                                String exceptionType = ((GetCredentialException) e).getType();
                                result.reject(
                                        new android.credentials.GetCredentialException(
                                                exceptionType, e.getMessage()));
                            } else {
                                result.reject(e);
                            }
                        }
                    }
                };

        GetDigitalCredentialOption option = new GetDigitalCredentialOption(request);
        client.getCredential(
                        new GetCredentialRequest(
                                Arrays.asList(
                                        new CredentialOption(
                                                option.getType(),
                                                option.getRequestData(),
                                                option.getCandidateQueryData(),
                                                request,
                                                "",
                                                "")),
                                new Bundle(),
                                origin,
                                resultReceiver))
                .addOnSuccessListener(
                        response -> {
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
                        });

        return result;
    }

    /**
     * Extracts a DigitalCredential from a response bundle.
     *
     * @param code The result code from the activity.
     * @param bundle The bundle containing the response data.
     * @return The extracted DigitalCredential.
     * @throws JSONException If there is an error parsing the JSON data.
     */
    @VisibleForTesting
    public static @Nullable DigitalCredential extractDigitalCredentialFromResponseBundle(
            int code, Bundle bundle) throws JSONException {
        Intent intent = IntentUtils.safeGetParcelable(bundle, BUNDLE_KEY_PROVIDER_DATA);
        if (intent == null) {
            return null;
        }
        var response = PendingIntentHandler.retrieveGetCredentialResponse(intent);
        if (response == null) {
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
        // TODO(crbug.com/336329411) Handle the case when the intent doesn't contain the
        // response, but contains an exception.
    }
}
