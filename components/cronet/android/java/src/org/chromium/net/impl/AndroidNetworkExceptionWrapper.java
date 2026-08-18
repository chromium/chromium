// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import androidx.annotation.RequiresApi;
import androidx.annotation.RequiresExtension;

import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.Set;

// Note we specify both RequiresApi and RequiresExtension because some older linters may only
// recognize the former.
@RequiresApi(EXT_API_LEVEL)
@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
class AndroidNetworkExceptionWrapper extends org.chromium.net.NetworkException {
    private final android.net.http.NetworkException mBackend;

    AndroidNetworkExceptionWrapper(android.net.http.NetworkException backend) {
        this(backend, false);
    }

    AndroidNetworkExceptionWrapper(
            android.net.http.NetworkException backend, boolean expectQuicException) {
        // Some Cronet users rely on the specific structure of native Cronet exceptions, especially
        // getCause(), which they expect to be null. For this reason, we cannot naively expose a
        // causal chain of wrapped exceptions. We expose a null cause instead, and attach the
        // backend exception as a suppressed exception so the full causal chain is preserved for
        // crash reporters and logging.
        super(backend.getMessage(), /* cause= */ null);
        this.mBackend = backend;
        configureException(this, backend);

        if (!expectQuicException && backend instanceof android.net.http.QuicException) {
            throw new IllegalArgumentException(
                    "Translating QuicException as NetworkException results in loss of information. "
                            + "Make sure you handle QuicException first. See the stacktrace "
                            + "for where the translation is being performed, and the cause "
                            + "for the exception being translated.",
                    backend);
        }
    }

    static void configureException(Throwable wrapper, Throwable backend) {
        wrapper.addSuppressed(backend);
        Throwable deepestCause = backend;
        Set<Throwable> seen = Collections.newSetFromMap(new IdentityHashMap<>());
        while (deepestCause.getCause() != null && seen.add(deepestCause.getCause())) {
            deepestCause = deepestCause.getCause();
        }
        wrapper.setStackTrace(deepestCause.getStackTrace());
    }

    @Override
    public int getErrorCode() {
        return mBackend.getErrorCode();
    }

    @Override
    public int getCronetInternalErrorCode() {
        // This maps to `NetError.ERR_HTTPENGINE_PROVIDER_IN_USE`. We cannot directly reference that
        // because it would introduce a dependency between HttpEngineNativeProvider and code that
        // ships as part of Cronet's impl JAR.
        // LINT.IfChange(HTTPENGINE_PROVIDER_IN_USE)
        return -508;
        // LINT.ThenChange(//net/base/net_error_list.h:HTTPENGINE_PROVIDER_IN_USE)
    }

    @Override
    public boolean immediatelyRetryable() {
        return mBackend.isImmediatelyRetryable();
    }
}
