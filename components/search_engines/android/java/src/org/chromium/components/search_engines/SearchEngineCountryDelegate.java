// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.search_engines;

import android.content.Context;

import androidx.annotation.MainThread;

import org.chromium.base.Promise;

/** Placeholder delegate class to get device country. Implemented in the internal code. */
public abstract class SearchEngineCountryDelegate {
    @MainThread
    public SearchEngineCountryDelegate(Context context) {}

    /**
     * Returns a {@link Promise} that will be fulfilled with the device country code. The promise
     * may be rejected if unable to fetch device country code. Clients should implement proper
     * callbacks to handle rejection. The promise is guaranteed to contain a non-null string.
     */
    @MainThread
    public abstract Promise<String> getDeviceCountry();
}
