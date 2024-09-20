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
import org.chromium.components.ip_protection_auth.GetProxyConfigRequest;
import org.chromium.components.ip_protection_auth.GetProxyConfigResponse;
import org.chromium.components.ip_protection_auth.common.IErrorCode;
import org.chromium.components.ip_protection_auth.common.IIpProtectionAuthAndSignCallback;
import org.chromium.components.ip_protection_auth.common.IIpProtectionAuthService;
import org.chromium.components.ip_protection_auth.common.IIpProtectionGetInitialDataCallback;
import org.chromium.components.ip_protection_auth.common.IIpProtectionGetProxyConfigCallback;

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
                            Log.e(
                                    TAG,
                                    "expected service type %s, got: %s",
                                    EXPECTED_SERVICE_TYPE,
                                    request.getServiceType());
                            callback.reportError(
                                    IErrorCode.IP_PROTECTION_AUTH_SERVICE_TRANSIENT_ERROR);
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
                            Log.e(
                                    TAG,
                                    "expected oauth token %s, got: %s",
                                    TEST_STRING,
                                    request.getOauthToken());
                            callback.reportError(
                                    IErrorCode.IP_PROTECTION_AUTH_SERVICE_TRANSIENT_ERROR);
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

                @Override
                public void getProxyConfig(
                        byte[] bytes, IIpProtectionGetProxyConfigCallback callback) {
                    Log.i(TAG, "got getProxyConfig request");
                    try {
                        GetProxyConfigRequest request =
                                GetProxyConfigRequest.parser().parseFrom(bytes);
                        if (!request.getServiceType().equals(EXPECTED_SERVICE_TYPE)) {
                            Log.e(
                                    TAG,
                                    "expected service type %s, got: %s",
                                    EXPECTED_SERVICE_TYPE,
                                    request.getServiceType());
                            callback.reportError(
                                    IErrorCode.IP_PROTECTION_AUTH_SERVICE_TRANSIENT_ERROR);
                            return;
                        }
                        callback.reportResult(
                                GetProxyConfigResponse.newBuilder()
                                        .addProxyChain(
                                                GetProxyConfigResponse.ProxyChain.newBuilder()
                                                        .setProxyA(TEST_STRING))
                                        .build()
                                        .toByteArray());
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
