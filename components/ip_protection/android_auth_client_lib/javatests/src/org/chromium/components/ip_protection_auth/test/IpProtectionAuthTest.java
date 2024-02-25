// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth.test;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import android.content.Context;
import android.os.ConditionVariable;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.components.ip_protection_auth.IpProtectionAuthClient;
import org.chromium.components.ip_protection_auth.common.proto.IpProtectionAuthProtos.AuthAndSignRequest;
import org.chromium.components.ip_protection_auth.common.proto.IpProtectionAuthProtos.AuthAndSignResponse;
import org.chromium.components.ip_protection_auth.common.proto.IpProtectionAuthProtos.GetInitialDataRequest;
import org.chromium.components.ip_protection_auth.common.proto.IpProtectionAuthProtos.GetInitialDataResponse;

@MediumTest
@RunWith(AndroidJUnit4.class)
@Batch(Batch.UNIT_TESTS)
public final class IpProtectionAuthTest {
    IpProtectionAuthClient mIpProtectionClient;

    @Before
    public void setUp() throws Exception {
        Context context = ApplicationProvider.getApplicationContext();

        final ConditionVariable conditionVariable = new ConditionVariable();
        IpProtectionAuthClient.IpProtectionAuthServiceCallback callback =
                new IpProtectionAuthClient.IpProtectionAuthServiceCallback() {
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
        IpProtectionAuthClient.createConnectedInstanceForTestingAsync(context, callback);
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
        IpProtectionAuthClient.GetInitialDataCallback getInitialDataCallback =
                new IpProtectionAuthClient.GetInitialDataCallback() {
                    @Override
                    public void onResult(GetInitialDataResponse response) {
                        getInitialDataResponse[0] = response;
                        conditionVariable.open();
                    }

                    @Override
                    public void onError(byte[] error) {
                        fail("onError should not be called");
                    }
                };
        GetInitialDataRequest request =
                GetInitialDataRequest.newBuilder().setTestPayload("get initial data").build();
        mIpProtectionClient.getInitialData(request, getInitialDataCallback);
        assertThat(conditionVariable.block(5000)).isTrue();
        assertThat(getInitialDataResponse[0].getTestPayload()).isEqualTo("get initial data");
    }

    @Test
    public void authAndSignTest() throws Exception {
        final ConditionVariable conditionVariable = new ConditionVariable();
        // Using a 1-element array so that the reference is final and can be passed into the lambda.
        final AuthAndSignResponse[] authAndSignResponse = new AuthAndSignResponse[1];
        IpProtectionAuthClient.AuthAndSignCallback authAndSignCallback =
                new IpProtectionAuthClient.AuthAndSignCallback() {
                    @Override
                    public void onResult(AuthAndSignResponse response) {
                        authAndSignResponse[0] = response;
                        conditionVariable.open();
                    }

                    @Override
                    public void onError(byte[] bytes) {
                        fail("onError should not be called");
                    }
                };
        AuthAndSignRequest authAndSignRequest =
                AuthAndSignRequest.newBuilder().setTestPayload("auth and sign").build();
        mIpProtectionClient.authAndSign(authAndSignRequest, authAndSignCallback);
        assertThat(conditionVariable.block(5000)).isTrue();
        assertThat(authAndSignResponse[0].getTestPayload()).isEqualTo("auth and sign");
    }
}
