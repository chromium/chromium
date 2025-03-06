// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.credentials.CredentialManager;
import androidx.credentials.CredentialManagerCallback;
import androidx.credentials.GetCredentialRequest;
import androidx.credentials.GetCredentialResponse;
import androidx.credentials.PasswordCredential;
import androidx.credentials.exceptions.GetCredentialException;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;

import java.util.concurrent.Executor;

/** Tests for the ThirdPartyCredentialManagerBridge. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class ThirdPartyCredentialManagerBridgeTest {

    private static final long FAKE_RECEIVER_BRIDGE_POINTER = 7;
    private static final String USERNAME = "username";
    private static final String PASSWORD = "password";
    private static final String ORIGIN = "www.example.com";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private ThirdPartyCredentialManagerBridge.Natives mReceiverBridgeJniMock;
    @Mock private CredentialManager mCredentialManager;
    @Mock private GetCredentialException mGetCredentialException;

    private ThirdPartyCredentialManagerBridge mBridge;

    @Before
    public void setUp() {
        ThirdPartyCredentialManagerBridgeJni.setInstanceForTesting(mReceiverBridgeJniMock);
        mBridge = new ThirdPartyCredentialManagerBridge(FAKE_RECEIVER_BRIDGE_POINTER);
        mBridge.setCredentialManagerForTesting(mCredentialManager);
    }

    @Test
    public void testOnPasswordCredentialReceivedCalled() {
        PasswordCredential passwordCredential = new PasswordCredential(USERNAME, PASSWORD);
        GetCredentialResponse response = new GetCredentialResponse(passwordCredential);

        doAnswer(invocation -> respondToCallback(invocation, response, null))
                .when(mCredentialManager)
                .getCredentialAsync(
                        any(Context.class),
                        any(GetCredentialRequest.class),
                        any(),
                        any(Executor.class),
                        any(CredentialManagerCallback.class));

        mBridge.get(ORIGIN);

        verify(mCredentialManager)
                .getCredentialAsync(
                        any(Context.class),
                        any(GetCredentialRequest.class),
                        any(),
                        any(Executor.class),
                        any(CredentialManagerCallback.class));
        verify(mReceiverBridgeJniMock)
                .onPasswordCredentialReceived(
                        FAKE_RECEIVER_BRIDGE_POINTER, USERNAME, PASSWORD, ORIGIN);
    }

    @Test
    public void testOnGetCredentialErrorCalled() {
        doAnswer(invocation -> respondToCallback(invocation, null, mGetCredentialException))
                .when(mCredentialManager)
                .getCredentialAsync(
                        any(Context.class),
                        any(GetCredentialRequest.class),
                        any(),
                        any(Executor.class),
                        any(CredentialManagerCallback.class));

        mBridge.get(ORIGIN);

        verify(mCredentialManager)
                .getCredentialAsync(
                        any(Context.class),
                        any(GetCredentialRequest.class),
                        any(),
                        any(Executor.class),
                        any(CredentialManagerCallback.class));
        verify(mReceiverBridgeJniMock).onGetPasswordCredentialError(FAKE_RECEIVER_BRIDGE_POINTER);
    }

    private Object respondToCallback(
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
}
