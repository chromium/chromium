// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.environment_integrity;

import androidx.concurrent.futures.CallbackToFutureAdapter;

import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.components.environment_integrity.enums.IntegrityResponse;

/**
 * Upstream implementation of {@link IntegrityServiceBridgeDelegate}.
 *
 * This will be replaced in downstream builds through build system magic.
 */
public class IntegrityServiceBridgeDelegateImpl implements IntegrityServiceBridgeDelegate {
    private static final String NOT_SUPPORTED_ERROR = "Environment Integrity not available.";
    @Override
    public ListenableFuture<Long> createEnvironmentIntegrityHandle(
            boolean ignored, int timeoutMilliseconds) {
        return CallbackToFutureAdapter.getFuture(resolver -> {
            resolver.setException(new IntegrityException(
                    NOT_SUPPORTED_ERROR, IntegrityResponse.API_NOT_AVAILABLE));
            return "IntegrityServiceBridgeDelegateImpl.createEnvironmentIntegrityHandle";
        });
    }

    @Override
    public ListenableFuture<byte[]> getEnvironmentIntegrityToken(
            long handle, byte[] requestHash, int timeoutMilliseconds) {
        return CallbackToFutureAdapter.getFuture(resolver -> {
            resolver.setException(new IntegrityException(
                    NOT_SUPPORTED_ERROR, IntegrityResponse.API_NOT_AVAILABLE));
            return "IntegrityServiceBridgeDelegateImpl.getEnvironmentIntegrityToken";
        });
    }
    @Override
    public boolean canUseGms() {
        return false;
    }
}
