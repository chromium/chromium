// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth.test;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import android.os.ConditionVariable;

import androidx.test.filters.MediumTest;

import com.google.privacy.ppn.proto.AuthAndSignRequest;
import com.google.privacy.ppn.proto.AuthAndSignResponse;
import com.google.privacy.ppn.proto.GetInitialDataRequest;
import com.google.privacy.ppn.proto.GetInitialDataResponse;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.ip_protection_auth.IpProtectionAuthClient;
import org.chromium.components.ip_protection_auth.IpProtectionAuthServiceCallback;
import org.chromium.components.ip_protection_auth.IpProtectionByteArrayCallback;

@MediumTest
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public final class IpProtectionAuthTest {
    IpProtectionAuthClient mIpProtectionClient;

    @Before
    public void setUp() throws Exception {
        LibraryLoader.getInstance().ensureInitialized();
        final ConditionVariable conditionVariable = new ConditionVariable();
        IpProtectionAuthServiceCallback callback =
                new IpProtectionAuthServiceCallback() {
                    @Override
                    public void onResult(IpProtectionAuthClient client) {
                        mIpProtectionClient = client;
                        conditionVariable.open();
                    }

                    @Override
                    public void onError(String error) {
                        fail("onError should not be called");
                    }
                };
        IpProtectionAuthClient.createConnectedInstanceForTestingAsync(callback);
        assertThat(conditionVariable.block(5000)).isTrue();
    }

    @After
    public void tearDown() {
        mIpProtectionClient.close();
    }

    @Test
    public void getInitialDataTest() throws Exception {
        final ConditionVariable conditionVariable = new ConditionVariable();
        // Using a 1-element array so that the reference is final and can be passed into the lambda.
        final GetInitialDataResponse[] getInitialDataResponse = new GetInitialDataResponse[1];
        IpProtectionByteArrayCallback getInitialDataCallback =
                new IpProtectionByteArrayCallback() {
                    @Override
                    public void onResult(byte[] response) {
                        try {
                            getInitialDataResponse[0] = GetInitialDataResponse.parseFrom(response);
                        } catch (Exception e) {
                            fail(e.getMessage());
                        }
                        conditionVariable.open();
                    }

                    @Override
                    public void onError(byte[] error) {
                        fail("onError should not be called");
                    }
                };
        GetInitialDataRequest request =
                GetInitialDataRequest.newBuilder().setServiceType("webviewipblinding").build();
        mIpProtectionClient.getInitialData(request.toByteArray(), getInitialDataCallback);
        assertThat(conditionVariable.block(5000)).isTrue();
    }

    @Test
    public void authAndSignTest() throws Exception {
        final ConditionVariable conditionVariable = new ConditionVariable();
        // Using a 1-element array so that the reference is final and can be passed into the lambda.
        final AuthAndSignResponse[] authAndSignResponse = new AuthAndSignResponse[1];
        IpProtectionByteArrayCallback authAndSignCallback =
                new IpProtectionByteArrayCallback() {
                    @Override
                    public void onResult(byte[] response) {
                        try {
                            authAndSignResponse[0] = AuthAndSignResponse.parseFrom(response);
                        } catch (Exception e) {
                            fail(e.getMessage());
                        }
                        conditionVariable.open();
                    }

                    @Override
                    public void onError(byte[] bytes) {
                        fail("onError should not be called");
                    }
                };
        AuthAndSignRequest authAndSignRequest =
                AuthAndSignRequest.newBuilder().setOauthToken("test").build();
        mIpProtectionClient.authAndSign(authAndSignRequest.toByteArray(), authAndSignCallback);
        assertThat(conditionVariable.block(5000)).isTrue();
    }

    @Test
    public void nativeCreateConnectedInstanceTest() throws Exception {
        IpProtectionAuthTestNatives.createConnectedInstanceForTesting();
    }

    @Test
    public void nativeGetInitialDataTest() throws Exception {
        IpProtectionAuthTestNatives.testGetInitialData();
    }

    @Test
    public void nativeAuthAndSignTest() throws Exception {
        IpProtectionAuthTestNatives.testAuthAndSign();
    }
}
