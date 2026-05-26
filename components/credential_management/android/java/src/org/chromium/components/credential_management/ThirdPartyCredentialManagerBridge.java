// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.credential_management;

import android.app.Activity;
import android.os.CancellationSignal;

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
import org.jni_zero.JniType;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.List;

/** A bridge for interacting with Credential Manager. */
@JNINamespace("credential_management")
@NullMarked
class ThirdPartyCredentialManagerBridge {
    private static @Nullable CredentialManager sCredentialManagerForTesting;
    private @Nullable CancellationSignal mCancellationSignal;

    @CalledByNative
    ThirdPartyCredentialManagerBridge() {}

    void setCredentialManagerForTesting(CredentialManager credentialManager) {
        sCredentialManagerForTesting = credentialManager;
        ResettersForTesting.register(() -> sCredentialManagerForTesting = null);
    }

    @CalledByNative
    void get(
            @Nullable WebContents webContents,
            boolean isAutoSelectAllowed,
            boolean includePasswords,
            @JniType("std::vector") List<GURL> federations,
            String origin,
            Callback<PasswordCredentialResponse> callback) {
        // TODO(crbug.com/419810756): Add support for federated credentials.
        Activity activity = getActivity(webContents);
        if (activity == null) {
            callback.onResult(new PasswordCredentialResponse(false, "", ""));
            return;
        }

        CredentialManager credentialManager =
                sCredentialManagerForTesting == null
                        ? CredentialManager.create(activity)
                        : sCredentialManagerForTesting;
        // We're currently preventing silent access for every get request by
        // default in 3rd party mode so isAutoSelectAllowed is always set to
        // false.
        GetPasswordOption passwordOption =
                new GetPasswordOption(
                        Collections.emptySet(),
                        /* isAutoSelectAllowed= */ false,
                        Collections.emptySet());
        GetCredentialRequest.Builder getCredentialRequestBuilder =
                new GetCredentialRequest.Builder();
        if (includePasswords) {
            getCredentialRequestBuilder.addCredentialOption(passwordOption);
        }
        getCredentialRequestBuilder.setOrigin(origin);

        CredentialManagerCallback<GetCredentialResponse, GetCredentialException>
                credentialCallback =
                        new CredentialManagerCallback<>() {
                            @Override
                            public void onError(GetCredentialException error) {
                                mCancellationSignal = null;
                                callback.onResult(new PasswordCredentialResponse(false, "", ""));
                                ThirdPartyCredentialManagerMetricsRecorder
                                        .recordCredentialManagerGetResult(
                                                /* success= */ false, /* error= */ error);
                            }

                            @Override
                            public void onResult(GetCredentialResponse result) {
                                mCancellationSignal = null;
                                onGetCredentialResponse(result, callback);
                                ThirdPartyCredentialManagerMetricsRecorder
                                        .recordCredentialManagerGetResult(
                                                /* success= */ true, /* error= */ null);
                            }
                        };
        mCancellationSignal = new CancellationSignal();
        credentialManager.getCredentialAsync(
                activity,
                getCredentialRequestBuilder.build(),
                mCancellationSignal,
                ThreadUtils::postOnUiThread,
                credentialCallback);
    }

    @CalledByNative
    void store(
            @Nullable WebContents webContents,
            String username,
            String password,
            String origin,
            Callback<Boolean> callback) {
        Activity activity = getActivity(webContents);
        if (activity == null) {
            callback.onResult(false);
            return;
        }

        CredentialManager credentialManager =
                sCredentialManagerForTesting == null
                        ? CredentialManager.create(activity)
                        : sCredentialManagerForTesting;
        CreatePasswordRequest createPasswordRequest =
                new CreatePasswordRequest(username, password, origin, false, false);

        CredentialManagerCallback<CreateCredentialResponse, CreateCredentialException>
                credentialCallback =
                        new CredentialManagerCallback<>() {
                            @Override
                            public void onError(CreateCredentialException error) {
                                mCancellationSignal = null;
                                callback.onResult(false);
                                ThirdPartyCredentialManagerMetricsRecorder
                                        .recordCredentialManagerStoreResult(
                                                /* success= */ false, /* error= */ error);
                            }

                            @Override
                            public void onResult(CreateCredentialResponse response) {
                                mCancellationSignal = null;
                                callback.onResult(true);
                                ThirdPartyCredentialManagerMetricsRecorder
                                        .recordCredentialManagerStoreResult(
                                                /* success= */ true, /* error= */ null);
                            }
                        };
        mCancellationSignal = new CancellationSignal();
        credentialManager.createCredentialAsync(
                activity,
                createPasswordRequest,
                mCancellationSignal,
                ThreadUtils::postOnUiThread,
                credentialCallback);
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

    private @Nullable Activity getActivity(@Nullable WebContents webContents) {
        if (webContents == null) {
            return null;
        }
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        return window != null ? window.getActivity().get() : null;
    }

    @CalledByNative
    void cancel() {
        if (mCancellationSignal != null) {
            mCancellationSignal.cancel();
            mCancellationSignal = null;
        }
    }
}
