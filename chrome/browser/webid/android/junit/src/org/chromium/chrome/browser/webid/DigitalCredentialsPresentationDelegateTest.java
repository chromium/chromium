// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.webid.DigitalCredentialsPresentationDelegate.BUNDLE_KEY_IDENTITY_TOKEN;
import static org.chromium.chrome.browser.webid.DigitalCredentialsPresentationDelegate.BUNDLE_KEY_PROVIDER_DATA;
import static org.chromium.chrome.browser.webid.DigitalCredentialsPresentationDelegate.BUNDLE_KEY_REQUEST_JSON;
import static org.chromium.chrome.browser.webid.DigitalCredentialsPresentationDelegate.EXTRA_CREDENTIAL_DATA;
import static org.chromium.chrome.browser.webid.DigitalCredentialsPresentationDelegate.EXTRA_GET_CREDENTIAL_RESPONSE;

import android.app.Activity;
import android.content.Intent;
import android.credentials.Credential;
import android.credentials.GetCredentialResponse;
import android.os.Build;
import android.os.Bundle;

import com.google.android.gms.identitycredentials.GetCredentialException;

import org.json.JSONException;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.webid.IdentityCredentialsDelegate.DigitalCredential;

/** Unit tests for {@link DigitalCredentialsPresentationDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        sdk = {Build.VERSION_CODES.TIRAMISU, Build.VERSION_CODES.UPSIDE_DOWN_CAKE})
public class DigitalCredentialsPresentationDelegateTest {
    private static final String INTENT_HELPER_EXTRA_CREDENTIAL_TYPE =
            "androidx.identitycredentials.EXTRA_CREDENTIAL_TYPE";
    private static final String INTENT_HELPER_EXTRA_CREDENTIAL_DATA =
            "androidx.identitycredentials.EXTRA_CREDENTIAL_DATA";
    private static final String JSON_PROTOCOL = "openid4vp";
    private static final String JSON_DATA = "{\"test_key\":\"test_value\"}";
    private static final String JSON_WITH_PROTOCOL =
            "{\"protocol\" : \"openid4vp\", \"data\": {\"test_key\":\"test_value\"}}";
    private static final String JSON_WITHOUT_PROTOCOL = "{\"data\": {\"test_key\":\"test_value\"}}";
    private static final byte[] LEGACY_RESPONSE = "{\"legacy_key\":\"legacy_value\"}".getBytes();

    private void packageResponseJsonInLegacyFormat(byte[] response, Intent intent) {
        Bundle dataBundle = new Bundle();
        dataBundle.putByteArray(BUNDLE_KEY_IDENTITY_TOKEN, response);

        intent.putExtra(INTENT_HELPER_EXTRA_CREDENTIAL_TYPE, "com.credman.IdentityCredential");
        intent.putExtra(INTENT_HELPER_EXTRA_CREDENTIAL_DATA, dataBundle);
    }

    private void packageResponseJsonInNewFormat(String json, Intent intent) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            packageResponseJsonInNewFormatAfter34(json, intent);
        } else {
            packageResponseJsonInNewFormatBefore34(json, intent);
        }
    }

    private void packageResponseJsonInNewFormatBefore34(String json, Intent intent) {
        Bundle credentialBundle = new Bundle();
        credentialBundle.putString(BUNDLE_KEY_REQUEST_JSON, json);

        Bundle dataBundle = new Bundle();
        dataBundle.putBundle(EXTRA_CREDENTIAL_DATA, credentialBundle);

        intent.putExtra(EXTRA_GET_CREDENTIAL_RESPONSE, dataBundle);
    }

    private void packageResponseJsonInNewFormatAfter34(String json, Intent intent) {
        Bundle credentialBundle = new Bundle();
        credentialBundle.putString(BUNDLE_KEY_REQUEST_JSON, json);

        // Credential and GetCredentialResponse classes don't exist in Robolectric test and hence we
        // mock them.
        Credential credential = mock(Credential.class);
        when(credential.getType()).thenReturn("androidx.credentials.TYPE_DIGITAL_CREDENTIAL");
        when(credential.getData()).thenReturn(credentialBundle);

        GetCredentialResponse getCredentialResponse = mock(GetCredentialResponse.class);
        when(getCredentialResponse.getCredential()).thenReturn(credential);

        intent.putExtra(EXTRA_GET_CREDENTIAL_RESPONSE, getCredentialResponse);
    }

    private Bundle packageIntentInResponseBundle(Intent intent) {
        Bundle bundle = new Bundle();
        bundle.putParcelable(BUNDLE_KEY_PROVIDER_DATA, intent);
        return bundle;
    }

    @Test
    public void testExtractDigitalCredentialFromGetResponse_LegacyFormat()
            throws JSONException, NullPointerException, GetCredentialException {
        Intent intent = new Intent();
        packageResponseJsonInLegacyFormat(LEGACY_RESPONSE, intent);
        Bundle bundle = packageIntentInResponseBundle(intent);

        DigitalCredential credential =
                DigitalCredentialsPresentationDelegate.extractDigitalCredentialFromResponseBundle(
                        Activity.RESULT_OK, bundle);

        assertNotNull(credential);
        assertNull(credential.mProtocol);
        assertEquals(new String(LEGACY_RESPONSE), credential.mData);
    }

    @Test
    public void testExtractDigitalCredentialFromGetResponse_NewFormat_ProcotolInResponse()
            throws JSONException, NullPointerException, GetCredentialException {
        Intent intent = new Intent();
        packageResponseJsonInNewFormat(JSON_WITH_PROTOCOL, intent);
        Bundle bundle = packageIntentInResponseBundle(intent);

        DigitalCredential extractedCredential =
                DigitalCredentialsPresentationDelegate.extractDigitalCredentialFromResponseBundle(
                        Activity.RESULT_OK, bundle);

        assertNotNull(extractedCredential);
        assertEquals(JSON_PROTOCOL, extractedCredential.mProtocol);
        assertEquals(JSON_DATA, extractedCredential.mData);
    }

    @Test
    public void testExtractDigitalCredentialFromGetResponse_BothFormats_ProcotolInResponse()
            throws JSONException, NullPointerException, GetCredentialException {
        Intent intent = new Intent();
        packageResponseJsonInNewFormat(JSON_WITH_PROTOCOL, intent);
        packageResponseJsonInLegacyFormat(LEGACY_RESPONSE, intent);
        Bundle bundle = packageIntentInResponseBundle(intent);

        DigitalCredential extractedCredential =
                DigitalCredentialsPresentationDelegate.extractDigitalCredentialFromResponseBundle(
                        Activity.RESULT_OK, bundle);

        // Since the modern format contains a protocol, it is preferred.
        assertNotNull(extractedCredential);
        assertEquals(JSON_PROTOCOL, extractedCredential.mProtocol);
        assertEquals(JSON_DATA, extractedCredential.mData);
    }

    @Test
    public void testExtractDigitalCredentialFromGetResponse_BothFormats_NoProcotolInResponse()
            throws JSONException, NullPointerException, GetCredentialException {
        Intent intent = new Intent();
        packageResponseJsonInNewFormat(JSON_WITHOUT_PROTOCOL, intent);
        packageResponseJsonInLegacyFormat(LEGACY_RESPONSE, intent);
        Bundle bundle = packageIntentInResponseBundle(intent);

        DigitalCredential extractedCredential =
                DigitalCredentialsPresentationDelegate.extractDigitalCredentialFromResponseBundle(
                        Activity.RESULT_OK, bundle);

        // Since the modern format doesn't contain a protocol, the full response is considered as
        // the data, and no protocol is returned.
        assertNotNull(extractedCredential);
        assertNull(extractedCredential.mProtocol);
        assertEquals(new String(JSON_WITHOUT_PROTOCOL), extractedCredential.mData);
    }

    @Test
    public void testExtractDigitalCredentialFromGetResponse_NullData_Legacy()
            throws JSONException, NullPointerException, GetCredentialException {
        Intent intent = new Intent();
        packageResponseJsonInLegacyFormat(null, intent);
        Bundle bundle = packageIntentInResponseBundle(intent);

        assertThrows(
                NullPointerException.class,
                () -> {
                    DigitalCredentialsPresentationDelegate
                            .extractDigitalCredentialFromResponseBundle(Activity.RESULT_OK, bundle);
                });
    }

    @Test
    public void testExtractDigitalCredentialFromGetResponse_JSONException()
            throws JSONException, NullPointerException, GetCredentialException {
        Intent intent = new Intent();
        packageResponseJsonInNewFormat("{invalidjson", intent);
        Bundle bundle = packageIntentInResponseBundle(intent);

        assertThrows(
                JSONException.class,
                () -> {
                    DigitalCredentialsPresentationDelegate
                            .extractDigitalCredentialFromResponseBundle(Activity.RESULT_OK, bundle);
                });
    }
}
