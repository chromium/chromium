// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import android.content.Context;

import org.chromium.net.CronetEngine;
import org.chromium.net.CronetProvider;
import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.ICronetEngineBuilder;
import org.chromium.net.impl.ImplVersion;

import java.util.Arrays;

/**
 * Implementation of {@link CronetProvider} that creates {@link CronetEngine.Builder}
 * for building the Fake implementation of {@link CronetEngine}.
 * {@hide}
 */
public class FakeCronetProvider extends CronetProvider {
    /**
     * String returned by {@link CronetProvider#getName} for {@link CronetProvider}
     * that provides the fake Cronet implementation.
     */
    public static final String PROVIDER_NAME_FAKE = "Fake-Cronet-Provider";

    /**
     * Constructs a {@link FakeCronetProvider}.
     *
     * @param context Android context to use
     */
    public FakeCronetProvider(Context context) {
        super(context);
    }

    @Override
    public CronetEngine.Builder createBuilder() {
        ICronetEngineBuilder impl = new FakeCronetEngine.Builder(mContext);
        return new ExperimentalCronetEngine.Builder(impl);
    }

    @Override
    public String getName() {
        return PROVIDER_NAME_FAKE;
    }

    @Override
    public String getVersion() {
        return ImplVersion.getCronetVersion();
    }

    @Override
    public boolean isEnabled() {
        return true;
    }

    @Override
    public int hashCode() {
        return Arrays.hashCode(new Object[] {FakeCronetProvider.class, mContext});
    }

    @Override
    public boolean equals(Object other) {
        return other == this
                || (other instanceof FakeCronetProvider
                        && this.mContext.equals(((FakeCronetProvider) other).mContext));
    }
}
