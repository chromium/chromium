// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.credentials.Credential;
import androidx.credentials.CredentialManager;
import androidx.credentials.CredentialManagerCallback;
import androidx.credentials.GetCredentialRequest;
import androidx.credentials.GetCredentialResponse;
import androidx.credentials.GetPasswordOption;
import androidx.credentials.PasswordCredential;
import androidx.credentials.exceptions.GetCredentialException;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;

/** A bridge for interacting with Credential Manager. */
@JNINamespace("android_autofill")
class ThirdPartyCredentialManagerBridge {
    private final long mReceiverBridge;
    private static CredentialManager sCredentialManagerForTesting;

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
        GetPasswordOption getPasswordRequest = new GetPasswordOption();
        GetCredentialRequest request =
                new GetCredentialRequest.Builder()
                        .addCredentialOption(getPasswordRequest)
                        .setOrigin(origin)
                        .build();

        CredentialManagerCallback<GetCredentialResponse, GetCredentialException>
                credentialCallback =
                        new CredentialManagerCallback<>() {
                            @Override
                            public void onError(@NonNull GetCredentialException e) {
                                onGetCredentialError();
                            }

                            @Override
                            public void onResult(@NonNull GetCredentialResponse result) {
                                onGetCredentialResponse(result, origin);
                            }
                        };
        credentialManager.getCredentialAsync(
                context, request, null, Runnable::run, credentialCallback);
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

        void onGetPasswordCredentialError(long nativeThirdPartyCredentialManagerBridge);
    }
}
