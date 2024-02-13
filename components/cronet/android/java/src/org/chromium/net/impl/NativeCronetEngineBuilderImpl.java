// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.content.Context;
import android.os.SystemClock;

import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.ICronetEngineBuilder;

import java.util.concurrent.atomic.AtomicLong;

/** Implementation of {@link ICronetEngineBuilder} that builds native Cronet engine. */
// WARNING: the fully qualified name of this class is hardcoded in the Google Play Services Cronet
// provider code, which is part of the Google Play Services SDK. This means THIS CLASS CANNOT BE
// RENAMED, MOVED NOR DELETED without breaking the Google Play Services provider.
public class NativeCronetEngineBuilderImpl extends CronetEngineBuilderImpl {
    private static final AtomicLong sLogCronetInitializationRef = new AtomicLong(0);

    /**
     * Builder for Native Cronet Engine.
     * Default config enables SPDY, disables QUIC and HTTP cache.
     *
     * @param context Android {@link Context} for engine to use.
     */
    public NativeCronetEngineBuilderImpl(Context context) {
        super(context);
    }

    @Override
    protected long getLogCronetInitializationRef() {
        sLogCronetInitializationRef.compareAndSet(0, mLogger.generateId());
        return sLogCronetInitializationRef.get();
    }

    @Override
    public ExperimentalCronetEngine build() {
        var startUptimeMillis = SystemClock.uptimeMillis();

        if (getUserAgent() == null) {
            setUserAgent(getDefaultUserAgent());
        }

        ExperimentalCronetEngine builder = new CronetUrlRequestContext(this, startUptimeMillis);

        // Clear MOCK_CERT_VERIFIER reference if there is any, since
        // the ownership has been transferred to the engine.
        mMockCertVerifier = 0;

        return builder;
    }
}
