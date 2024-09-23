// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.content.Context;
import android.net.http.HttpEngine;
import android.os.Build;
import android.os.ext.SdkExtensions;

import androidx.annotation.RequiresExtension;

import org.chromium.net.CronetEngine;
import org.chromium.net.CronetProvider;
import org.chromium.net.ExperimentalCronetEngine;

/**
 * A Cronet provider implementation which loads the HttpEngine implementation from the Android
 * platform.
 *
 * <p>Note that the httpengine native provider doesn't provide functionality which was deemed to be
 *  too implementation specific, namely access to the netlog and internal metrics. Additionally,
 * support for experimental features is not guaranteed (as with any other Cronet provider).
 */
public class HttpEngineNativeProvider extends CronetProvider {
    /**
     * String returned by {@link CronetProvider#getName} for {@link CronetProvider} that provides
     * Cronet implementation based on the HttpEngine implementation present in the Platform. This
     * implementation doesn't provide functionality which was deemed to be implementation specific,
     * namely access to the netlog and internal metrics. Additionally, support for experimental
     * features is not guaranteed (as with any other Cronet provider).
     */
    // TODO(crbug.com/40287946): Move this to CronetProvider
    public static final String PROVIDER_NAME_HTTPENGINE_NATIVE = "HttpEngine-Native-Provider";

    static final int EXT_API_LEVEL = Build.VERSION_CODES.S;
    static final int EXT_VERSION = 7;

    public HttpEngineNativeProvider(Context context) {
        super(context);
    }

    @Override
    @RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
    public CronetEngine.Builder createBuilder() {
        return new ExperimentalCronetEngine.Builder(
                new AndroidHttpEngineBuilderWrapper(new HttpEngine.Builder(mContext)));
    }

    @Override
    public String getName() {
        return PROVIDER_NAME_HTTPENGINE_NATIVE;
    }

    @Override
    @RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
    public String getVersion() {
        return HttpEngine.getVersionString();
    }

    @Override
    public boolean isEnabled() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                && SdkExtensions.getExtensionVersion(EXT_API_LEVEL) >= EXT_VERSION;
    }
}
