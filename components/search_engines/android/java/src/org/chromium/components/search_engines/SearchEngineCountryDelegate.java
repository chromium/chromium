// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.search_engines;

import androidx.annotation.MainThread;

import org.chromium.base.Promise;

/** Delegate class to get device country. Implemented in the internal code. */
public interface SearchEngineCountryDelegate {
    /**
     * Returns a {@link Promise} that will be fulfilled with the device country code. The promise
     * may be rejected if unable to fetch device country code.
     */
    @MainThread
    public Promise<String> getDeviceCountry();
}
