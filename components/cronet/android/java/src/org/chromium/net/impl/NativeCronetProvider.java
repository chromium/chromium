// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.content.Context;

import org.chromium.build.annotations.UsedByReflection;
import org.chromium.net.CronetEngine;
import org.chromium.net.CronetProvider;
import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.ICronetEngineBuilder;
import org.chromium.net.impl.CronetLogger.CronetSource;

import java.util.Arrays;

/**
 * Implementation of {@link CronetProvider} that creates {@link CronetEngine.Builder} for building
 * the native implementation of {@link CronetEngine}.
 */
public class NativeCronetProvider extends CronetProvider {
    public static final String OVERRIDE_NATIVE_CRONET_WITH_HTTPENGINE_FLAG =
            "Cronet_OverrideNativeCronetWithHttpEngine";
    private final HttpEngineNativeProvider mHttpEngineProvider;

    /**
     * Constructor.
     *
     * @param context Android context to use.
     */
    @UsedByReflection("CronetProvider.java")
    public NativeCronetProvider(Context context) {
        super(context);
        mHttpEngineProvider = new HttpEngineNativeProvider(mContext);
    }

    @Override
    public CronetEngine.Builder createBuilder() {
        if (shouldUseHttpEngine()) {
            return mHttpEngineProvider.createBuilder();
        } else {
            ICronetEngineBuilder impl =
                    new NativeCronetEngineBuilderWithLibraryLoaderImpl(mContext);
            return new ExperimentalCronetEngine.Builder(impl);
        }
    }

    @Override
    public String getName() {
        return CronetProvider.PROVIDER_NAME_APP_PACKAGED;
    }

    @Override
    public String getVersion() {
        return shouldUseHttpEngine()
                ? mHttpEngineProvider.getVersion()
                : ImplVersion.getCronetVersion();
    }

    @Override
    public boolean isEnabled() {
        return true;
    }

    @Override
    public int hashCode() {
        return Arrays.hashCode(new Object[] {NativeCronetProvider.class, mContext});
    }

    @Override
    public boolean equals(Object other) {
        return other == this
                || (other instanceof NativeCronetProvider
                        && this.mContext.equals(((NativeCronetProvider) other).mContext));
    }

    private boolean shouldUseHttpEngine() {
        if (!HttpEngineNativeProvider.isHttpEngineAvailable()) return false;
        var shouldForceHttpEngine =
                HttpFlagsForImpl.getHttpFlags(
                                mContext, CronetSource.CRONET_SOURCE_STATICALLY_LINKED)
                        .flags()
                        .get(OVERRIDE_NATIVE_CRONET_WITH_HTTPENGINE_FLAG);
        return shouldForceHttpEngine != null && shouldForceHttpEngine.getBoolValue();
    }
}
