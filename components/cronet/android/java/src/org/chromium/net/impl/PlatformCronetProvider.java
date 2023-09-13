// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.content.Context;
import android.net.http.HttpEngine;

import androidx.annotation.RequiresApi;
import androidx.core.os.BuildCompat;

import org.chromium.net.CronetEngine;
import org.chromium.net.CronetProvider;
import org.chromium.net.ExperimentalCronetEngine;

/**
 * A Cronet provider implementation which loads the implementation from the Android platform.
 *
 * <p>Note that the platform provider doesn't provide functionality which was deemed to be too
 * implementation specific, namely access to the netlog and internal metrics. Additionally, support
 * for experimental features is not guaranteed (as with any other Cronet provider).
 */
public class PlatformCronetProvider extends CronetProvider {
    public static final String PROVIDER_NAME_ANDROID_PLATFORM = "Android-Platform-Cronet-Provider";

    public PlatformCronetProvider(Context context) {
        super(context);
    }

    @Override
    @RequiresApi(34)
    public CronetEngine.Builder createBuilder() {
        return new ExperimentalCronetEngine.Builder(
                new AndroidHttpEngineBuilderWrapper(new HttpEngine.Builder(mContext)));
    }

    @Override
    public String getName() {
        return PROVIDER_NAME_ANDROID_PLATFORM;
    }

    @Override
    public String getVersion() {
        return HttpEngine.getVersionString();
    }

    @Override
    public boolean isEnabled() {
        return BuildCompat.isAtLeastU();
    }
}
