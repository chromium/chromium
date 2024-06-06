// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth.mock_service;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.RemoteException;

import androidx.annotation.Nullable;

import com.google.privacy.ppn.proto.AuthAndSignRequest;
import com.google.privacy.ppn.proto.AuthAndSignResponse;
import com.google.privacy.ppn.proto.GetInitialDataRequest;
import com.google.privacy.ppn.proto.GetInitialDataResponse;
import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.Log;
import org.chromium.components.ip_protection_auth.common.IIpProtectionAuthAndSignCallback;
import org.chromium.components.ip_protection_auth.common.IIpProtectionAuthService;
import org.chromium.components.ip_protection_auth.common.IIpProtectionGetInitialDataCallback;

/** Mock implementation of the IP Protection Auth Service */
public final class IpProtectionAuthServiceMock extends Service {
    private static final String TAG = "IppAuthServiceMock";
    private static final String TEST_STRING = "test";
    private static final String EXPECTED_SERVICE_TYPE = "webviewipblinding";

    private final IIpProtectionAuthService.Stub mBinder =
            new IIpProtectionAuthService.Stub() {
                @Override
                public void getInitialData(
                        byte[] bytes, IIpProtectionGetInitialDataCallback callback) {
                    Log.i(TAG, "got getInitialData request");
                    try {
                        GetInitialDataRequest request =
                                GetInitialDataRequest.parser().parseFrom(bytes);
                        if (!request.getServiceType().equals(EXPECTED_SERVICE_TYPE)) {
                            String errorMessage =
                                    "expected service type "
                                            + EXPECTED_SERVICE_TYPE
                                            + ", got: "
                                            + request.getServiceType();
                            callback.reportError(errorMessage.getBytes());
                            return;
                        }
                        GetInitialDataResponse.PrivacyPassData privacyPassData =
                                GetInitialDataResponse.PrivacyPassData.newBuilder()
                                        .setTokenKeyId(ByteString.copyFrom(TEST_STRING.getBytes()))
                                        .build();
                        GetInitialDataResponse response =
                                GetInitialDataResponse.newBuilder()
                                        .setPrivacyPassData(privacyPassData)
                                        .build();
                        callback.reportResult(response.toByteArray());
                    } catch (RemoteException ex) {
                        // TODO(abhijithnair): Handle this exception correctly.
                        throw new RuntimeException(ex);
                    } catch (InvalidProtocolBufferException ex) {
                        // TODO(abhijithnair): Handle this exception correctly.
                        throw new RuntimeException(ex);
                    }
                }

                @Override
                public void authAndSign(byte[] bytes, IIpProtectionAuthAndSignCallback callback) {
                    Log.i(TAG, "got authAndSign request");
                    try {
                        AuthAndSignRequest request = AuthAndSignRequest.parser().parseFrom(bytes);
                        if (!request.getOauthToken().equals(TEST_STRING)) {
                            String errorMessage =
                                    "expected oauth token "
                                            + TEST_STRING
                                            + ", got: "
                                            + request.getOauthToken();
                            callback.reportError(errorMessage.getBytes());
                            return;
                        }
                        AuthAndSignResponse response =
                                AuthAndSignResponse.newBuilder().setApnType(TEST_STRING).build();
                        callback.reportResult(response.toByteArray());
                    } catch (RemoteException ex) {
                        // TODO(abhijithnair): Handle this exception correctly.
                        throw new RuntimeException(ex);
                    } catch (InvalidProtocolBufferException ex) {
                        // TODO(abhijithnair): Handle this exception correctly.
                        throw new RuntimeException(ex);
                    }
                }
            };

    @Override
    public void onCreate() {
        Log.i(TAG, "onCreate");
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "onDestroy");
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        Log.i(TAG, "returning binding for %s", intent.toString());
        return mBinder;
    }
}
