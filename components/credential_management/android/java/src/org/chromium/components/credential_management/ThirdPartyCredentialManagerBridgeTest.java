// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.credential_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.credentials.CreateCredentialResponse;
import androidx.credentials.CreatePasswordRequest;
import androidx.credentials.CreatePasswordResponse;
import androidx.credentials.CredentialManager;
import androidx.credentials.CredentialManagerCallback;
import androidx.credentials.GetCredentialRequest;
import androidx.credentials.GetCredentialResponse;
import androidx.credentials.PasswordCredential;
import androidx.credentials.exceptions.CreateCredentialException;
import androidx.credentials.exceptions.GetCredentialException;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;

import java.util.concurrent.Executor;

/** Tests for the ThirdPartyCredentialManagerBridge. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class ThirdPartyCredentialManagerBridgeTest {

    private static final String USERNAME = "username";
    private static final String PASSWORD = "password";
    private static final String ORIGIN = "www.example.com";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private CredentialManager mCredentialManager;
    @Mock private GetCredentialException mGetCredentialException;
    @Mock private CreateCredentialException mCreateCredentialException;
    @Mock private Callback<PasswordCredentialResponse> mCredentialResponseCallback;
    @Mock private Callback<Boolean> mStoreCallback;

    private ThirdPartyCredentialManagerBridge mBridge;

    @Before
    public void setUp() {
        mBridge = new ThirdPartyCredentialManagerBridge();
        mBridge.setCredentialManagerForTesting(mCredentialManager);
    }

    @Test
    public void testOnPasswordCredentialReceivedCalled() {
        PasswordCredential passwordCredential = new PasswordCredential(USERNAME, PASSWORD);
        GetCredentialResponse response = new GetCredentialResponse(passwordCredential);

        doAnswer(invocation -> respondToGetCallback(invocation, response, null))
                .when(mCredentialManager)
                .getCredentialAsync(
                        any(Context.class),
                        any(GetCredentialRequest.class),
                        any(),
                        any(Executor.class),
                        any(CredentialManagerCallback.class));

        mBridge.get(true, true, ORIGIN, mCredentialResponseCallback);

        verify(mCredentialManager)
                .getCredentialAsync(
                        any(Context.class),
                        any(GetCredentialRequest.class),
                        any(),
                        any(Executor.class),
                        any(CredentialManagerCallback.class));
        verify(mCredentialResponseCallback)
                .onResult(
                        argThat(
                                new PasswordCredentialResponseMatcher(
                                        new PasswordCredentialResponse(true, USERNAME, PASSWORD))));
    }

    @Test
    public void testOnGetCredentialErrorCalled() {
        doAnswer(invocation -> respondToGetCallback(invocation, null, mGetCredentialException))
                .when(mCredentialManager)
                .getCredentialAsync(
                        any(Context.class),
                        any(GetCredentialRequest.class),
                        any(),
                        any(Executor.class),
                        any(CredentialManagerCallback.class));

        mBridge.get(false, true, ORIGIN, mCredentialResponseCallback);

        verify(mCredentialManager)
                .getCredentialAsync(
                        any(Context.class),
                        any(GetCredentialRequest.class),
                        any(),
                        any(Executor.class),
                        any(CredentialManagerCallback.class));
        verify(mCredentialResponseCallback)
                .onResult(
                        argThat(
                                new PasswordCredentialResponseMatcher(
                                        new PasswordCredentialResponse(false, "", ""))));
    }

    @Test
    public void testOnCreateCredentialSucceeds() {
        CreateCredentialResponse response = new CreatePasswordResponse();
        doAnswer(invocation -> respondToStoreCallback(invocation, response, null))
                .when(mCredentialManager)
                .createCredentialAsync(
                        any(Context.class),
                        any(CreatePasswordRequest.class),
                        any(),
                        any(Executor.class),
                        any(CredentialManagerCallback.class));

        mBridge.store(USERNAME, PASSWORD, ORIGIN, mStoreCallback);

        verify(mCredentialManager)
                .createCredentialAsync(
                        any(Context.class),
                        any(CreatePasswordRequest.class),
                        any(),
                        any(Executor.class),
                        any(CredentialManagerCallback.class));
        verify(mStoreCallback).onResult(true);
    }

    @Test
    public void testonCreateCredentialFails() {
        doAnswer(invocation -> respondToStoreCallback(invocation, null, mCreateCredentialException))
                .when(mCredentialManager)
                .createCredentialAsync(
                        any(Context.class),
                        any(CreatePasswordRequest.class),
                        any(),
                        any(Executor.class),
                        any(CredentialManagerCallback.class));

        mBridge.store(USERNAME, PASSWORD, ORIGIN, mStoreCallback);

        verify(mCredentialManager)
                .createCredentialAsync(
                        any(Context.class),
                        any(CreatePasswordRequest.class),
                        any(),
                        any(Executor.class),
                        any(CredentialManagerCallback.class));
        verify(mStoreCallback).onResult(false);
    }

    private Object respondToGetCallback(
            InvocationOnMock invocation,
            GetCredentialResponse response,
            GetCredentialException exception) {
        Executor executor =
                (Executor) invocation.getArgument(3); // Get the Executor for the callback.
        CredentialManagerCallback<GetCredentialResponse, GetCredentialException> callback =
                (CredentialManagerCallback<GetCredentialResponse, GetCredentialException>)
                        invocation.getArgument(4); // Get the callback argument.
        executor.execute(
                () -> {
                    if (response != null) {
                        callback.onResult(response);
                    } else {
                        callback.onError(exception);
                    }
                });
        return null;
    }

    private Object respondToStoreCallback(
            InvocationOnMock invocation,
            CreateCredentialResponse response,
            CreateCredentialException exception) {
        Executor executor =
                (Executor) invocation.getArgument(3); // Get the Executor for the callback.
        CredentialManagerCallback<CreateCredentialResponse, CreateCredentialException> callback =
                (CredentialManagerCallback<CreateCredentialResponse, CreateCredentialException>)
                        invocation.getArgument(4); // Get the callback argument.
        executor.execute(
                () -> {
                    if (response != null) {
                        callback.onResult(response);
                    } else {
                        callback.onError(exception);
                    }
                });
        return null;
    }

    static class PasswordCredentialResponseMatcher
            implements ArgumentMatcher<PasswordCredentialResponse> {
        private final PasswordCredentialResponse mExpected;

        public PasswordCredentialResponseMatcher(PasswordCredentialResponse expected) {
            mExpected = expected;
        }

        @Override
        public boolean matches(PasswordCredentialResponse actual) {
            return actual != null
                    && actual.getSuccess() == mExpected.getSuccess()
                    && actual.getUsername().equals(mExpected.getUsername())
                    && actual.getPassword().equals(mExpected.getPassword());
        }
    }
}
