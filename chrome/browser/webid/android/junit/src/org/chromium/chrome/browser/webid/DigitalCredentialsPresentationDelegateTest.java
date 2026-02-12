// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThrows;

import static org.chromium.chrome.browser.webid.DigitalCredentialsPresentationDelegate.BUNDLE_KEY_PROVIDER_DATA;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;

import androidx.credentials.GetCredentialResponse;
import androidx.credentials.provider.PendingIntentHandler;

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

    private Bundle packageIntentInResponseBundle(Intent intent) {
        Bundle bundle = new Bundle();
        bundle.putParcelable(BUNDLE_KEY_PROVIDER_DATA, intent);
        return bundle;
    }

    @Test
    public void testExtractDigitalCredentialFromGetResponse() throws JSONException {
        Intent intent = new Intent();
        packageResponseJson(JSON_WITH_PROTOCOL, intent);
        Bundle bundle = packageIntentInResponseBundle(intent);

        DigitalCredential extractedCredential =
                DigitalCredentialsPresentationDelegate.extractDigitalCredentialFromResponseBundle(
                        Activity.RESULT_OK, bundle);

        assertNotNull(extractedCredential);
        assertEquals(JSON_PROTOCOL, extractedCredential.mProtocol);
        assertEquals(JSON_DATA, extractedCredential.mData);
    }

    @Test
    public void testExtractDigitalCredentialFromGetResponse_NoProtocol() throws JSONException {
        Intent intent = new Intent();
        packageResponseJson(JSON_WITHOUT_PROTOCOL, intent);
        Bundle bundle = packageIntentInResponseBundle(intent);

        assertThrows(
                JSONException.class,
                () -> {
                    DigitalCredentialsPresentationDelegate
                            .extractDigitalCredentialFromResponseBundle(Activity.RESULT_OK, bundle);
                });
    }
}
