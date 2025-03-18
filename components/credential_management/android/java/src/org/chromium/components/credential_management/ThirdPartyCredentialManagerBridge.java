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
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A bridge for interacting with Credential Manager. */
@JNINamespace("credential_management")
@NullMarked
class ThirdPartyCredentialManagerBridge {
    private final long mReceiverBridge;
    private static @Nullable CredentialManager sCredentialManagerForTesting;

    @CalledByNative
    ThirdPartyCredentialManagerBridge(long receiverBridge) {
        mReceiverBridge = receiverBridge;
    }

    void setCredentialManagerForTesting(CredentialManager credentialManager) {
        sCredentialManagerForTesting = credentialManager;
        ResettersForTesting.register(() -> sCredentialManagerForTesting = null);
    }

    @CalledByNative
    void get(String origin) {
        Context context = ContextUtils.getApplicationContext();
        CredentialManager credentialManager =
                sCredentialManagerForTesting == null
                        ? CredentialManager.create(context)
                        : sCredentialManagerForTesting;
        GetPasswordOption passwordOption = new GetPasswordOption();
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
                                onGetCredentialError();
                            }

                            @Override
                            public void onResult(GetCredentialResponse result) {
                                onGetCredentialResponse(result, origin);
                            }
                        };
        credentialManager.getCredentialAsync(
                context, getPasswordRequest, null, Runnable::run, credentialCallback);
    }

    @CalledByNative
    void store(String username, String password, String origin) {
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
                                onCreateCredentialResponse(false);
                            }

                            @Override
                            public void onResult(CreateCredentialResponse response) {
                                onCreateCredentialResponse(true);
                            }
                        };
        credentialManager.createCredentialAsync(
                context, createPasswordRequest, null, Runnable::run, credentialCallback);
    }

    private void onGetCredentialResponse(GetCredentialResponse result, String origin) {
        Credential credential = result.getCredential();
        assert credential instanceof PasswordCredential;
        PasswordCredential passwordCredential = (PasswordCredential) credential;
        String username = passwordCredential.getId();
        String password = passwordCredential.getPassword();
        if (username != null && password != null) {
            ThirdPartyCredentialManagerBridgeJni.get()
                    .onPasswordCredentialReceived(mReceiverBridge, username, password, origin);
        }
    }

    private void onCreateCredentialResponse(boolean success) {
        ThirdPartyCredentialManagerBridgeJni.get()
                .onCreateCredentialResponse(mReceiverBridge, success);
    }

    private void onGetCredentialError() {
        ThirdPartyCredentialManagerBridgeJni.get().onGetPasswordCredentialError(mReceiverBridge);
    }

    @NativeMethods
    interface Natives {
        void onPasswordCredentialReceived(
                long nativeThirdPartyCredentialManagerBridge,
                String username,
                String password,
                String origin);

        void onCreateCredentialResponse(
                long nativeThirdPartyCredentialManagerBridge, boolean success);

        void onGetPasswordCredentialError(long nativeThirdPartyCredentialManagerBridge);
    }
}
