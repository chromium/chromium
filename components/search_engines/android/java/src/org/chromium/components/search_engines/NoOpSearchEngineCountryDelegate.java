// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.search_engines;

import androidx.annotation.MainThread;

import org.chromium.base.LocaleUtils;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;

/** Placeholder implementation for public code. */
@NullMarked
public class NoOpSearchEngineCountryDelegate extends SearchEngineCountryDelegate {
    @MainThread
    public NoOpSearchEngineCountryDelegate() {
        ThreadUtils.assertOnUiThread();
    }

    @Override
    @MainThread
    public Promise<String> getDeviceCountry() {
        ThreadUtils.assertOnUiThread();
        return Promise.fulfilled(LocaleUtils.getDefaultCountryCode());
    }
}
