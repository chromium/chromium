// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import android.net.http.HttpEngine;

import androidx.annotation.RequiresExtension;

import org.chromium.base.Log;
import org.chromium.net.CronetEngine;
import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.ICronetEngineBuilder;

import java.util.Date;
import java.util.Set;

@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
class AndroidHttpEngineBuilderWrapper extends ICronetEngineBuilder {
    private static final String TAG = "HttpEngBuilderWrap";

    private final HttpEngine.Builder mBackend;

    public AndroidHttpEngineBuilderWrapper(HttpEngine.Builder backend) {
        this.mBackend = backend;
    }

    @Override
    public String getDefaultUserAgent() {
        return mBackend.getDefaultUserAgent();
    }

    @Override
    public ICronetEngineBuilder setUserAgent(String userAgent) {
        mBackend.setUserAgent(userAgent);
        return this;
    }

    @Override
    public ICronetEngineBuilder setStoragePath(String value) {
        mBackend.setStoragePath(value);
        return this;
    }

    @Override
    public ICronetEngineBuilder setLibraryLoader(CronetEngine.Builder.LibraryLoader loader) {
        Log.w(
                TAG,
                "Custom library loader isn't supported when using the platform Cronet provider."
                        + " Ignoring...");
        return this;
    }

    @Override
    public ICronetEngineBuilder enableQuic(boolean value) {
        mBackend.setEnableQuic(value);
        return this;
    }

    @Override
    public ICronetEngineBuilder enableSdch(boolean value) {
        // Deprecated and unused by upper layers, do nothing.
        return this;
    }

    @Override
    public ICronetEngineBuilder enableHttp2(boolean value) {
        mBackend.setEnableHttp2(value);
        return this;
    }

    @Override
    public ICronetEngineBuilder enableBrotli(boolean value) {
        mBackend.setEnableBrotli(value);
        return this;
    }

    @Override
    public ICronetEngineBuilder setExperimentalOptions(String options) {
        // TODO(danstahr): Hidden API. This should ideally extract values we know how to handle as a
        // main API
        return this;
    }

    @Override
    public ICronetEngineBuilder enableHttpCache(int cacheMode, long maxSize) {
        mBackend.setEnableHttpCache(cacheMode, maxSize);
        return this;
    }

    @Override
    public ICronetEngineBuilder addQuicHint(String host, int port, int alternatePort) {
        mBackend.addQuicHint(host, port, alternatePort);
        return this;
    }

    @Override
    public ICronetEngineBuilder addPublicKeyPins(
            String hostName,
            Set<byte[]> pinsSha256,
            boolean includeSubdomains,
            Date expirationDate) {
        mBackend.addPublicKeyPins(
                hostName, pinsSha256, includeSubdomains, expirationDate.toInstant());
        return this;
    }

    @Override
    public ICronetEngineBuilder enablePublicKeyPinningBypassForLocalTrustAnchors(boolean value) {
        mBackend.setEnablePublicKeyPinningBypassForLocalTrustAnchors(value);
        return this;
    }

    /**
     * Build a {@link CronetEngine} using this builder's configuration.
     *
     * @return constructed {@link CronetEngine}.
     */
    @Override
    public ExperimentalCronetEngine build() {
        return new AndroidHttpEngineWrapper(mBackend.build());
    }
}
