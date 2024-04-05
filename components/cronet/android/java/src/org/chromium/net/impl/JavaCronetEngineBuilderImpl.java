// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.content.Context;

import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.ICronetEngineBuilder;
import org.chromium.net.impl.CronetLogger.CronetSource;

import java.util.concurrent.atomic.AtomicLong;

/** Implementation of {@link ICronetEngineBuilder} that builds Java-based Cronet engine. */
public class JavaCronetEngineBuilderImpl extends CronetEngineBuilderImpl {
    private static final AtomicLong sLogCronetInitializationRef = new AtomicLong(0);

    /**
     * Builder for Platform Cronet Engine.
     *
     * @param context Android {@link Context} for engine to use.
     */
    public JavaCronetEngineBuilderImpl(Context context) {
        super(context, CronetSource.CRONET_SOURCE_FALLBACK);
    }

    @Override
    protected long getLogCronetInitializationRef() {
        sLogCronetInitializationRef.compareAndSet(0, mLogger.generateId());
        return sLogCronetInitializationRef.get();
    }

    @Override
    public ExperimentalCronetEngine build() {
        if (getUserAgent() == null) {
            setUserAgent(getDefaultUserAgent());
        }
        return new JavaCronetEngine(this);
    }
}
