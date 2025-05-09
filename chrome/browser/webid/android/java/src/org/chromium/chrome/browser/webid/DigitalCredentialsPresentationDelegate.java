// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import static androidx.core.app.ActivityCompat.startIntentSenderForResult;

import android.app.Activity;
import android.content.Intent;
import android.content.IntentSender.SendIntentException;
import android.credentials.GetCredentialResponse;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ResultReceiver;

import androidx.annotation.OptIn;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.credentials.GetDigitalCredentialOption;

import com.google.android.gms.identitycredentials.CredentialOption;
import com.google.android.gms.identitycredentials.GetCredentialException;
import com.google.android.gms.identitycredentials.GetCredentialRequest;
import com.google.android.gms.identitycredentials.IdentityCredentialClient;
import com.google.android.gms.identitycredentials.IdentityCredentialManager;
import com.google.android.gms.identitycredentials.IntentHelper;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.webid.IdentityCredentialsDelegate.DigitalCredential;

import java.util.Arrays;
import java.util.Objects;

@NullMarked
public class DigitalCredentialsPresentationDelegate {
    private static final String TAG = "DCPresentDelegate";

    // Arbitrary request code that is used when invoking the GMSCore API.
    private static final int REQUEST_CODE_DIGITAL_CREDENTIALS = 777;

    @VisibleForTesting
    public static final String BUNDLE_KEY_REQUEST_JSON =
            "androidx.credentials.BUNDLE_KEY_REQUEST_JSON";

    @VisibleForTesting public static final String DC_API_RESPONSE_PROTOCOL_KEY = "protocol";
    @VisibleForTesting public static final String DC_API_RESPONSE_DATA_KEY = "data";
    @VisibleForTesting public static final String BUNDLE_KEY_IDENTITY_TOKEN = "identityToken";

    @VisibleForTesting
    public static final String BUNDLE_KEY_PROVIDER_DATA =
            "androidx.identitycredentials.BUNDLE_KEY_PROVIDER_DATA";

    @VisibleForTesting
    public static final String EXTRA_GET_CREDENTIAL_RESPONSE =
            "android.service.credentials.extra.GET_CREDENTIAL_RESPONSE";

    @VisibleForTesting
    public static final String EXTRA_CREDENTIAL_DATA =
            "androidx.credentials.provider.extra.EXTRA_CREDENTIAL_DATA";

    @OptIn(markerClass = androidx.credentials.ExperimentalDigitalCredentialApi.class)
    public Promise<DigitalCredential> get(Activity window, String origin, String request) {
        final IdentityCredentialClient client;
        try {
            client = IdentityCredentialManager.Companion.getClient(window);
        } catch (Exception e) {
            // Thrown when running in phones without the most current GMS
            // version.
            return Promise.rejected();
        }

        final Promise<DigitalCredential> result = new Promise<DigitalCredential>();

        ResultReceiver resultReceiver =
                new ResultReceiver(new Handler(Looper.getMainLooper())) {
                    // android.credentials.GetCredentialException requires API level 34
                    @SuppressWarnings("NewApi")
                    @Override
                    protected void onReceiveResult(int code, Bundle data) {
                        Log.d(TAG, "Received a response");
                        try {
                            result.fulfill(extractDigitalCredentialFromResponseBundle(code, data));
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
                            try {
                                Log.d(TAG, "Sending an intent for sender");
                                Log.d(TAG, request);
                                startIntentSenderForResult(
                                        /* activity= */ window,
                                        /* intent= */ response.getPendingIntent().getIntentSender(),
                                        REQUEST_CODE_DIGITAL_CREDENTIALS,
                                        /* fillInIntent= */ null,
                                        /* flagsMask= */ 0,
                                        /* flagsValues= */ 0,
                                        /* extraFlags= */ 0,
                                        /* options= */ null);
                            } catch (SendIntentException e) {
                                Log.e(TAG, "Sending an intent for sender failed");
                                result.reject(e);
                            }
                        });

        return result;
    }

    /**
     * Extracts a DigitalCredential from a response bundle.
     *
     * <p>This method attempts to extract a DigitalCredential from the given response bundle. It
     * first tries to parse the response in the new format. If that fails, it falls back to the
     * legacy format.
     *
     * @param code The result code from the activity.
     * @param bundle The bundle containing the response data.
     * @return The extracted DigitalCredential.
     * @throws JSONException If there is an error parsing the JSON data.
     * @throws NullPointerException If required data is missing in the legacy format.
     * @throws GetCredentialException If there is an issue with the credential.
     */
    @VisibleForTesting
    public static DigitalCredential extractDigitalCredentialFromResponseBundle(
            int code, Bundle bundle)
            throws JSONException, NullPointerException, GetCredentialException {
        // Try to read the new format.
        var digitalCredential = extractDigitalCredentialFromModernResponse(bundle);
        if (digitalCredential != null) {
            return digitalCredential;
        }
        // TODO(crbug.com/336329411) Handle the case when the intent doesn't contain the modern
        // response, but contains the modern exception.

        // Fallback to the legacy format.
        var response = IntentHelper.extractGetCredentialResponse(code, bundle);
        var token = response.getCredential().getData().getByteArray(BUNDLE_KEY_IDENTITY_TOKEN);

        return new DigitalCredential(null, Objects.requireNonNull(token));
    }

    /**
     * Extracts a DigitalCredential from a response bundle in the modern format.
     *
     * @param bundle The bundle containing the response data.
     * @return The extracted DigitalCredential, or null if the response is not in the modern format.
     * @throws JSONException If there is an error parsing the JSON data.
     */
    private static @Nullable DigitalCredential extractDigitalCredentialFromModernResponse(
            Bundle bundle) throws JSONException {
        Intent intent = IntentUtils.safeGetParcelable(bundle, BUNDLE_KEY_PROVIDER_DATA);
        if (intent == null) {
            return null;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            return extractDigitalCredentialIntentAfter34(intent);
        }
        return extractDigitalCredentialIntentBefore34(intent);
    }

    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    private static @Nullable DigitalCredential extractDigitalCredentialIntentAfter34(Intent intent)
            throws JSONException {
        GetCredentialResponse response =
                IntentUtils.safeGetParcelableExtra(intent, EXTRA_GET_CREDENTIAL_RESPONSE);
        if (response == null) {
            return null;
        }
        return extractDigitalCredentialFromCredentialDataBundle(response.getCredential().getData());
    }

    private static @Nullable DigitalCredential extractDigitalCredentialIntentBefore34(Intent intent)
            throws JSONException {
        Bundle responseBundle =
                IntentUtils.safeGetBundleExtra(intent, EXTRA_GET_CREDENTIAL_RESPONSE);
        if (responseBundle == null) {
            return null;
        }
        return extractDigitalCredentialFromCredentialDataBundle(
                IntentUtils.safeGetBundle(responseBundle, EXTRA_CREDENTIAL_DATA));
    }

    private static @Nullable DigitalCredential extractDigitalCredentialFromCredentialDataBundle(
            @Nullable Bundle bundle) throws JSONException {
        if (bundle == null) {
            return null;
        }
        String credentialJson = bundle.getString(BUNDLE_KEY_REQUEST_JSON);
        if (credentialJson == null) {
            return null;
        }
        JSONObject credential = new JSONObject(credentialJson);
        // Unless the json contains the protocol, return null to fallback to the legacy format.
        if (credential.has(DC_API_RESPONSE_PROTOCOL_KEY)) {
            String protocol = credential.getString(DC_API_RESPONSE_PROTOCOL_KEY);
            var data = credential.getJSONObject(DC_API_RESPONSE_DATA_KEY);
            return new DigitalCredential(protocol, data.toString());
        }
        // Otherwise, treat the whole json as the response. This is added for backward compatibility
        // where GMSCore was setting the modern response with the contents of the legacy response
        // without a protocol.
        return new DigitalCredential(null, credentialJson);
    }
}
