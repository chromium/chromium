// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.credential_management;

import android.content.Context;

import androidx.credentials.CreateCredentialResponse;
import androidx.credentials.CreatePasswordRequest;
import androidx.credentials.Credential;
import androidx.credentials.CredentialManager;
import androidx.credentials.CredentialManagerCallback;
import androidx.credentials.GetCredentialRequest;
import androidx.credentials.GetCredentialResponse;
import androidx.credentials.GetPasswordOption;
import androidx.credentials.PasswordCredential;
import androidx.credentials.exceptions.CreateCredentialException;
import androidx.credentials.exceptions.GetCredentialException;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Collections;

/** A bridge for interacting with Credential Manager. */
@JNINamespace("credential_management")
@NullMarked
class ThirdPartyCredentialManagerBridge {
    private static @Nullable CredentialManager sCredentialManagerForTesting;

    @CalledByNative
    ThirdPartyCredentialManagerBridge() {}

    void setCredentialManagerForTesting(CredentialManager credentialManager) {
        sCredentialManagerForTesting = credentialManager;
        ResettersForTesting.register(() -> sCredentialManagerForTesting = null);
    }

    @CalledByNative
    void get(
            boolean isAutoSelectAllowed,
            String origin,
            Callback<PasswordCredentialResponse> callback) {
        Context context = ContextUtils.getApplicationContext();
        CredentialManager credentialManager =
                sCredentialManagerForTesting == null
                        ? CredentialManager.create(context)
                        : sCredentialManagerForTesting;
        GetPasswordOption passwordOption =
                new GetPasswordOption(
                        Collections.emptySet(), isAutoSelectAllowed, Collections.emptySet());
        GetCredentialRequest getPasswordRequest =
                new GetCredentialRequest.Builder()
                        .addCredentialOption(passwordOption)
                        .setOrigin(origin)
                        .build();

        CredentialManagerCallback<GetCredentialResponse, GetCredentialException>
                credentialCallback =
                        new CredentialManagerCallback<>() {
                            @Override
                            public void onError(GetCredentialException e) {
                                callback.onResult(new PasswordCredentialResponse(false, "", ""));
                            }

                            @Override
                            public void onResult(GetCredentialResponse result) {
                                onGetCredentialResponse(result, callback);
                            }
                        };
        credentialManager.getCredentialAsync(
                context, getPasswordRequest, null, Runnable::run, credentialCallback);
    }

    @CalledByNative
    void store(String username, String password, String origin, Callback<Boolean> callback) {
        Context context = ContextUtils.getApplicationContext();
        CredentialManager credentialManager =
                sCredentialManagerForTesting == null
                        ? CredentialManager.create(context)
                        : sCredentialManagerForTesting;
        CreatePasswordRequest createPasswordRequest =
                new CreatePasswordRequest(username, password, origin, false, false);

        CredentialManagerCallback<CreateCredentialResponse, CreateCredentialException>
                credentialCallback =
                        new CredentialManagerCallback<>() {
                            @Override
                            public void onError(CreateCredentialException e) {
                                callback.onResult(false);
                            }

                            @Override
                            public void onResult(CreateCredentialResponse response) {
                                callback.onResult(true);
                            }
                        };
        credentialManager.createCredentialAsync(
                context, createPasswordRequest, null, Runnable::run, credentialCallback);
    }

    private void onGetCredentialResponse(
            GetCredentialResponse result, Callback<PasswordCredentialResponse> callback) {
        Credential credential = result.getCredential();
        assert credential instanceof PasswordCredential;
        PasswordCredential passwordCredential = (PasswordCredential) credential;
        String username = passwordCredential.getId();
        String password = passwordCredential.getPassword();
        PasswordCredentialResponse response =
                new PasswordCredentialResponse(true, username, password);
        callback.onResult(response);
    }
}
