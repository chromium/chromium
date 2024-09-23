// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.facilitated_payments;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;

/** Tests for the native bridge of the facilitated payment API client. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@SmallTest
public class FacilitatedPaymentsApiClientBridgeUnitTest {
    private static final long NATIVE_FACILITATED_PAYMENTS_API_CLIENT_ANDROID = 0x12345678;

    @Mock private FacilitatedPaymentsApiClientBridge.Natives mBridgeNatives;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(FacilitatedPaymentsApiClientBridgeJni.TEST_HOOKS, mBridgeNatives);
    }

    @After
    public void tearDown() {
        FacilitatedPaymentsApiClient.setFactory(null);
    }

    @Test
    public void apiIsNotAvailableByDefault() throws Exception {
        FacilitatedPaymentsApiClientBridge bridge =
                new FacilitatedPaymentsApiClientBridge(
                        NATIVE_FACILITATED_PAYMENTS_API_CLIENT_ANDROID,
                        /* renderFrameHost= */ null);

        bridge.isAvailable();

        verify(mBridgeNatives)
                .onIsAvailable(eq(NATIVE_FACILITATED_PAYMENTS_API_CLIENT_ANDROID), eq(false));
    }

    @Test
    public void cannotRetrieveClientTokenByDefault() throws Exception {
        FacilitatedPaymentsApiClientBridge bridge =
                new FacilitatedPaymentsApiClientBridge(
                        NATIVE_FACILITATED_PAYMENTS_API_CLIENT_ANDROID,
                        /* renderFrameHost= */ null);

        bridge.getClientToken();

        verify(mBridgeNatives)
                .onGetClientToken(eq(NATIVE_FACILITATED_PAYMENTS_API_CLIENT_ANDROID), eq(null));
    }

    @Test
    public void purchaseActionFailsByDefault() throws Exception {
        FacilitatedPaymentsApiClientBridge bridge =
                new FacilitatedPaymentsApiClientBridge(
                        NATIVE_FACILITATED_PAYMENTS_API_CLIENT_ANDROID,
                        /* renderFrameHost= */ null);

        bridge.invokePurchaseAction(
                /* primaryAccount= */ null, new byte[] {'A', 'c', 't', 'i', 'o', 'n'});

        verify(mBridgeNatives)
                .onPurchaseActionResultEnum(eq(NATIVE_FACILITATED_PAYMENTS_API_CLIENT_ANDROID),
                        eq(PurchaseActionResult.COULD_NOT_INVOKE));
    }

    @Test
    public void cannotCheckApiAvailabilityAfterNativePointerReset() throws Exception {
        FacilitatedPaymentsApiClientBridge bridge =
                new FacilitatedPaymentsApiClientBridge(
                        NATIVE_FACILITATED_PAYMENTS_API_CLIENT_ANDROID,
                        /* renderFrameHost= */ null);
        bridge.resetNativePointer();

        bridge.isAvailable();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void cannotRetrieveClientTokenAfterNativePointerReset() throws Exception {
        FacilitatedPaymentsApiClientBridge bridge =
                new FacilitatedPaymentsApiClientBridge(
                        NATIVE_FACILITATED_PAYMENTS_API_CLIENT_ANDROID,
                        /* renderFrameHost= */ null);
        bridge.resetNativePointer();

        bridge.getClientToken();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void cannotInvokePurchaseActionAfterNativePointerReset() throws Exception {
        FacilitatedPaymentsApiClientBridge bridge =
                new FacilitatedPaymentsApiClientBridge(
                        NATIVE_FACILITATED_PAYMENTS_API_CLIENT_ANDROID,
                        /* renderFrameHost= */ null);
        bridge.resetNativePointer();

        bridge.invokePurchaseAction(
                /* primaryAccount= */ null, new byte[] {'A', 'c', 't', 'i', 'o', 'n'});

        verifyNoInteractions(mBridgeNatives);
    }

    /**
     * If there is a mismatch between the public interface and the private implementation then the
     * JNI bridge continues to work, e.g., calling bridge.isAvailable() will not throw a
     * NullPointerException.
     */
    @Test
    public void mismatchedInterfaceAndImplementationCreatesNonNullApiClients() throws Exception {
        // A factory that does not override any of the interface methods. This simulates a mismatch
        // between the public interface and private implementation.
        FacilitatedPaymentsApiClient.setFactory(new FacilitatedPaymentsApiClient.Factory() {});

        FacilitatedPaymentsApiClientBridge bridge =
                new FacilitatedPaymentsApiClientBridge(
                        NATIVE_FACILITATED_PAYMENTS_API_CLIENT_ANDROID,
                        /* renderFrameHost= */ null);
        bridge.isAvailable();

        verify(mBridgeNatives)
                .onIsAvailable(eq(NATIVE_FACILITATED_PAYMENTS_API_CLIENT_ANDROID), eq(false));
    }
}
