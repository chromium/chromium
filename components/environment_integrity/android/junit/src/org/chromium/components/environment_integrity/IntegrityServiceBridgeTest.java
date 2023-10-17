// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.environment_integrity;

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.robolectric.Shadows.shadowOf;

import android.os.Looper;

import com.google.common.util.concurrent.ListenableFuture;
import com.google.common.util.concurrent.SettableFuture;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.environment_integrity.enums.IntegrityResponse;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests for {@link IntegrityServiceBridge}, asserting that JNI callback methods are called
 * correctly, with the correct callback ID.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class IntegrityServiceBridgeTest {
    @Rule public JniMocker mocker = new JniMocker();
    @Mock private IntegrityServiceBridge.Natives mIntegrityServiceBridgeNativesMock;

    private ShadowLooper mShadowLooper;

    private static final long NATIVE_CALLBACK_ID = 123456789L;
    private static final int TIMEOUT_MILLIS = 500;
    private static final long HANDLE = 987654321L;
    private static final byte[] CONTENT_BINDING = {0xA, 0xB, 0xC, 0xD};
    private static final byte[] TOKEN = {0xF, 0xE, 0xD, 0xC};

    private TestIntegrityServiceBridgeImpl mTestBridge;

    static class TestIntegrityServiceBridgeImpl implements IntegrityServiceBridgeDelegate {
        private final SettableFuture<Long> mHandleFuture = SettableFuture.create();
        private final SettableFuture<byte[]> mTokenFuture = SettableFuture.create();

        private final List<Boolean> mHandleRequestParams = new ArrayList<>();

        void resolveHandle(long handle) {
            mHandleFuture.set(handle);
        }

        void resolveHandleWithException(Exception exception) {
            mHandleFuture.setException(exception);
        }

        void resolveIntegrityToken(byte[] token) {
            mTokenFuture.set(token);
        }

        void resolveIntegrityWithException(Exception exception) {
            mTokenFuture.setException(exception);
        }

        public List<Boolean> getHandleRequestParams() {
            return Collections.unmodifiableList(mHandleRequestParams);
        }

        @Override
        public ListenableFuture<Long> createEnvironmentIntegrityHandle(
                boolean bindAppIdentity, int timeoutMilliseconds) {
            mHandleRequestParams.add(bindAppIdentity);
            return mHandleFuture;
        }

        @Override
        public ListenableFuture<byte[]> getEnvironmentIntegrityToken(
                long handle, byte[] requestHash, int timeoutMilliseconds) {
            return mTokenFuture;
        }

        @Override
        public boolean canUseGms() {
            return true;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(IntegrityServiceBridgeJni.TEST_HOOKS, mIntegrityServiceBridgeNativesMock);
        mTestBridge = new TestIntegrityServiceBridgeImpl();
        IntegrityServiceBridge.setDelegateForTesting(mTestBridge);
        mShadowLooper = shadowOf(Looper.getMainLooper());
    }

    @Test
    public void testGetNewHandle() {
        IntegrityServiceBridge.createHandle(NATIVE_CALLBACK_ID, TIMEOUT_MILLIS);
        mTestBridge.resolveHandle(HANDLE);
        waitForCallback();
        Mockito.verify(mIntegrityServiceBridgeNativesMock)
                .onCreateHandleResult(
                        eq(NATIVE_CALLBACK_ID),
                        eq(IntegrityResponse.SUCCESS),
                        eq(HANDLE),
                        eq(null));
    }

    @Test
    public void testHandleTimeout() {
        IntegrityServiceBridge.createHandle(NATIVE_CALLBACK_ID, TIMEOUT_MILLIS);
        mTestBridge.resolveHandleWithException(
                new IntegrityException("", IntegrityResponse.TIMEOUT));
        waitForCallback();
        Mockito.verify(mIntegrityServiceBridgeNativesMock)
                .onCreateHandleResult(
                        eq(NATIVE_CALLBACK_ID), eq(IntegrityResponse.TIMEOUT), eq(0L), anyString());
    }

    @Test
    public void testAppBindingEnabled() {
        ThreadUtils.runOnUiThreadBlocking(() -> IntegrityServiceBridge.setBindAppIdentity(true));
        IntegrityServiceBridge.createHandle(NATIVE_CALLBACK_ID, TIMEOUT_MILLIS);
        mTestBridge.resolveHandle(HANDLE);
        waitForCallback();

        final List<Boolean> handleRequestParams = mTestBridge.getHandleRequestParams();
        Assert.assertEquals(1, handleRequestParams.size());
        Assert.assertTrue("Handle must be app identity binding", handleRequestParams.get(0));
    }

    @Test
    public void testAppBindingDisabled() {
        ThreadUtils.runOnUiThreadBlocking(() -> IntegrityServiceBridge.setBindAppIdentity(false));
        IntegrityServiceBridge.createHandle(NATIVE_CALLBACK_ID, TIMEOUT_MILLIS);
        mTestBridge.resolveHandle(HANDLE);
        waitForCallback();

        final List<Boolean> handleRequestParams = mTestBridge.getHandleRequestParams();
        Assert.assertEquals(1, handleRequestParams.size());
        Assert.assertFalse("Handle must not be app identity binding", handleRequestParams.get(0));
    }

    @Test
    public void testHandleApiNotAvailableException() {
        IntegrityServiceBridge.createHandle(NATIVE_CALLBACK_ID, TIMEOUT_MILLIS);
        mTestBridge.resolveHandleWithException(
                new IntegrityException("Error", IntegrityResponse.API_NOT_AVAILABLE));
        waitForCallback();
        Mockito.verify(mIntegrityServiceBridgeNativesMock)
                .onCreateHandleResult(
                        eq(NATIVE_CALLBACK_ID),
                        eq(IntegrityResponse.API_NOT_AVAILABLE),
                        eq(0L),
                        anyString());
    }

    @Test
    public void testHandleRetryExponentialBackoffException() {
        IntegrityServiceBridge.createHandle(NATIVE_CALLBACK_ID, TIMEOUT_MILLIS);
        mTestBridge.resolveHandleWithException(
                new IntegrityException("Error", IntegrityResponse.TIMEOUT));
        waitForCallback();
        Mockito.verify(mIntegrityServiceBridgeNativesMock)
                .onCreateHandleResult(
                        eq(NATIVE_CALLBACK_ID), eq(IntegrityResponse.TIMEOUT), eq(0L), anyString());
    }

    @Test
    public void testHandleRuntimeException() {
        IntegrityServiceBridge.createHandle(NATIVE_CALLBACK_ID, TIMEOUT_MILLIS);
        mTestBridge.resolveHandleWithException(new RuntimeException());
        waitForCallback();
        Mockito.verify(mIntegrityServiceBridgeNativesMock)
                .onCreateHandleResult(
                        eq(NATIVE_CALLBACK_ID),
                        eq(IntegrityResponse.UNKNOWN_ERROR),
                        eq(0L),
                        anyString());
    }

    @Test
    public void testGetIntegrityToken() {
        IntegrityServiceBridge.getIntegrityToken(
                NATIVE_CALLBACK_ID, HANDLE, CONTENT_BINDING, TIMEOUT_MILLIS);
        mTestBridge.resolveIntegrityToken(TOKEN);
        waitForCallback();
        Mockito.verify(mIntegrityServiceBridgeNativesMock)
                .onGetIntegrityTokenResult(
                        eq(NATIVE_CALLBACK_ID), eq(IntegrityResponse.SUCCESS), eq(TOKEN), eq(null));
    }

    @Test
    public void testIntegrityTokenTimeout() {
        IntegrityServiceBridge.getIntegrityToken(
                NATIVE_CALLBACK_ID, HANDLE, CONTENT_BINDING, TIMEOUT_MILLIS);
        mTestBridge.resolveIntegrityWithException(
                new IntegrityException("", IntegrityResponse.TIMEOUT));
        waitForCallback();
        Mockito.verify(mIntegrityServiceBridgeNativesMock)
                .onGetIntegrityTokenResult(
                        eq(NATIVE_CALLBACK_ID),
                        eq(IntegrityResponse.TIMEOUT),
                        eq(null),
                        anyString());
    }

    @Test
    public void testIntegrityTokenInvalidHandleException() {
        IntegrityServiceBridge.getIntegrityToken(
                NATIVE_CALLBACK_ID, HANDLE, CONTENT_BINDING, TIMEOUT_MILLIS);
        mTestBridge.resolveIntegrityWithException(
                new IntegrityException("", IntegrityResponse.INVALID_HANDLE));
        waitForCallback();
        Mockito.verify(mIntegrityServiceBridgeNativesMock)
                .onGetIntegrityTokenResult(
                        eq(NATIVE_CALLBACK_ID),
                        eq(IntegrityResponse.INVALID_HANDLE),
                        eq(null),
                        anyString());
    }

    @Test
    public void testIntegrityTokenExponentialBackoffException() {
        IntegrityServiceBridge.getIntegrityToken(
                NATIVE_CALLBACK_ID, HANDLE, CONTENT_BINDING, TIMEOUT_MILLIS);
        mTestBridge.resolveIntegrityWithException(
                new IntegrityException("", IntegrityResponse.TIMEOUT));
        waitForCallback();
        Mockito.verify(mIntegrityServiceBridgeNativesMock)
                .onGetIntegrityTokenResult(
                        eq(NATIVE_CALLBACK_ID),
                        eq(IntegrityResponse.TIMEOUT),
                        eq(null),
                        anyString());
    }

    @Test
    public void testIntegrityTokenRuntimeException() {
        IntegrityServiceBridge.getIntegrityToken(
                NATIVE_CALLBACK_ID, HANDLE, CONTENT_BINDING, TIMEOUT_MILLIS);
        mTestBridge.resolveIntegrityWithException(new RuntimeException());
        waitForCallback();
        Mockito.verify(mIntegrityServiceBridgeNativesMock)
                .onGetIntegrityTokenResult(
                        eq(NATIVE_CALLBACK_ID),
                        eq(IntegrityResponse.UNKNOWN_ERROR),
                        eq(null),
                        anyString());
    }

    private void waitForCallback() {
        mShadowLooper.idle();
    }
}
