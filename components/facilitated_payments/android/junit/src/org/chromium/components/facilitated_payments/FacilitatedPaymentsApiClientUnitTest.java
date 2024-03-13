// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.facilitated_payments;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;

/** Tests for the facilitated payment API client. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@SmallTest
public class FacilitatedPaymentsApiClientUnitTest {
    private static final byte[] TEST_CLIENT_TOKEN = new byte[] {'C', 'l', 'i', 'e', 'n', 't'};

    @After
    public void tearDown() {
        FacilitatedPaymentsApiClient.setFactory(null);
    }

    /** A delegate for receiving the responses from the API. */
    public class TestDelegate implements FacilitatedPaymentsApiClient.Delegate {
        public boolean mIsAvailableChecked;
        public boolean mIsAvailable;

        public boolean mIsClientTokenRetrieved;
        public byte[] mClientToken;

        public boolean mIsPurchaseActionInvoked;
        public boolean mIsPurchaseActionSuccessful;

        @Override
        public void onIsAvailable(boolean isAvailable) {
            mIsAvailableChecked = true;
            mIsAvailable = isAvailable;
        }

        @Override
        public void onGetClientToken(byte[] clientToken) {
            mIsClientTokenRetrieved = true;
            mClientToken = clientToken;
        }

        @Override
        public void onPurchaseActionResult(boolean isPurchaseActionSuccessful) {
            mIsPurchaseActionInvoked = true;
            mIsPurchaseActionSuccessful = isPurchaseActionSuccessful;
        }
    }

    @Test
    public void apiIsNotAvailableByDefault() throws Exception {
        TestDelegate delegate = new TestDelegate();
        FacilitatedPaymentsApiClient apiClient = FacilitatedPaymentsApiClient.create(delegate);

        apiClient.isAvailable();

        Assert.assertTrue(delegate.mIsAvailableChecked);
        Assert.assertFalse(delegate.mIsAvailable);
    }

    @Test
    public void cannotRetrieveClientTokenByDefault() throws Exception {
        TestDelegate delegate = new TestDelegate();
        FacilitatedPaymentsApiClient apiClient = FacilitatedPaymentsApiClient.create(delegate);

        apiClient.getClientToken();

        Assert.assertTrue(delegate.mIsClientTokenRetrieved);
        Assert.assertNull(delegate.mClientToken);
    }

    @Test
    public void purchaseActionFailsByDefault() throws Exception {
        TestDelegate delegate = new TestDelegate();
        FacilitatedPaymentsApiClient apiClient = FacilitatedPaymentsApiClient.create(delegate);

        apiClient.invokePurchaseAction(new byte[] {'A', 'c', 't', 'i', 'o', 'n'});

        Assert.assertTrue(delegate.mIsPurchaseActionInvoked);
        Assert.assertFalse(delegate.mIsPurchaseActionSuccessful);
    }

    /** A fake implementation of the API client, which always succeeds. */
    public class FakeApiClient extends FacilitatedPaymentsApiClient {
        /** Creates an instance of a fake implementation of the API client. */
        public FakeApiClient(Delegate delegate) {
            super(delegate);
        }

        @Override
        public void isAvailable() {
            mDelegate.onIsAvailable(/* isAvailable= */ true);
        }

        @Override
        public void getClientToken() {
            mDelegate.onGetClientToken(/* clientToken= */ TEST_CLIENT_TOKEN);
        }

        @Override
        public void invokePurchaseAction(byte[] actionToken) {
            mDelegate.onPurchaseActionResult(/* isPurchaseActionSuccessful= */ true);
        }
    }

    /** A factory for creating a fake implementation of the API client, which always succeeds. */
    public class FakeApiClientFactory implements FacilitatedPaymentsApiClient.Factory {
        @Override
        public FacilitatedPaymentsApiClient factoryCreate(
                FacilitatedPaymentsApiClient.Delegate delegate) {
            return new FakeApiClient(delegate);
        }
    }

    @Test
    public void factoryCanOverrideApiAvailableResult() throws Exception {
        FacilitatedPaymentsApiClient.setFactory(new FakeApiClientFactory());
        TestDelegate delegate = new TestDelegate();
        FacilitatedPaymentsApiClient apiClient = FacilitatedPaymentsApiClient.create(delegate);

        apiClient.isAvailable();

        Assert.assertTrue(delegate.mIsAvailableChecked);
        Assert.assertTrue(delegate.mIsAvailable);
    }

    @Test
    public void factoryCanOverrideClientTokenResult() throws Exception {
        FacilitatedPaymentsApiClient.setFactory(new FakeApiClientFactory());
        TestDelegate delegate = new TestDelegate();
        FacilitatedPaymentsApiClient apiClient = FacilitatedPaymentsApiClient.create(delegate);

        apiClient.getClientToken();

        Assert.assertTrue(delegate.mIsClientTokenRetrieved);
        Assert.assertArrayEquals(TEST_CLIENT_TOKEN, delegate.mClientToken);
    }

    @Test
    public void factoryCanOverridePurchaseActionResult() throws Exception {
        FacilitatedPaymentsApiClient.setFactory(new FakeApiClientFactory());
        TestDelegate delegate = new TestDelegate();
        FacilitatedPaymentsApiClient apiClient = FacilitatedPaymentsApiClient.create(delegate);

        apiClient.invokePurchaseAction(new byte[] {'A', 'c', 't', 'i', 'o', 'n'});

        Assert.assertTrue(delegate.mIsPurchaseActionInvoked);
        Assert.assertTrue(delegate.mIsPurchaseActionSuccessful);
    }
}
