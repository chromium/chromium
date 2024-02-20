// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth.mock_service;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.RemoteException;

import androidx.annotation.Nullable;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.components.ip_protection_auth.common.IIpProtectionAuthAndSignCallback;
import org.chromium.components.ip_protection_auth.common.IIpProtectionAuthService;
import org.chromium.components.ip_protection_auth.common.IIpProtectionGetInitialDataCallback;
import org.chromium.components.ip_protection_auth.common.proto.IpProtectionAuthProtos.AuthAndSignRequest;
import org.chromium.components.ip_protection_auth.common.proto.IpProtectionAuthProtos.AuthAndSignResponse;
import org.chromium.components.ip_protection_auth.common.proto.IpProtectionAuthProtos.GetInitialDataRequest;
import org.chromium.components.ip_protection_auth.common.proto.IpProtectionAuthProtos.GetInitialDataResponse;

/** Mock implementation of the IP Protection Auth Service */
public final class IpProtectionAuthServiceMock extends Service {
    private static final String TAG = "IpProtectionAuthServiceMock";
    private final IIpProtectionAuthService.Stub mBinder =
            new IIpProtectionAuthService.Stub() {
                // TODO(abhijithnair): Currently this method just passes the same request byte[]
                // back as the result. Use a mock result instead.
                @Override
                public void getInitialData(
                        byte[] bytes, IIpProtectionGetInitialDataCallback callback) {
                    try {
                        GetInitialDataRequest request =
                                GetInitialDataRequest.parser().parseFrom(bytes);
                        GetInitialDataResponse response =
                                GetInitialDataResponse.newBuilder()
                                        .setTestPayload(request.getTestPayload())
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

                // TODO(abhijithnair): Currently this method just passes the same request byte[]
                // back as the result. Use a mock result instead.
                @Override
                public void authAndSign(byte[] bytes, IIpProtectionAuthAndSignCallback callback) {
                    try {
                        AuthAndSignRequest request = AuthAndSignRequest.parser().parseFrom(bytes);
                        AuthAndSignResponse response =
                                AuthAndSignResponse.newBuilder()
                                        .setTestPayload(request.getTestPayload())
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
            };

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
}
