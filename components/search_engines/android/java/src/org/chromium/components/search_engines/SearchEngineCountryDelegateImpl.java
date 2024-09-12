// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.search_engines;

import android.content.Context;

import androidx.annotation.MainThread;

import org.chromium.base.LocaleUtils;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;

/** Placeholder implementation for public code. */
public class SearchEngineCountryDelegateImpl extends SearchEngineCountryDelegate {
    @MainThread
    public SearchEngineCountryDelegateImpl(Context context) {
        super(context);
        ThreadUtils.assertOnUiThread();
    }

    @Override
    @MainThread
    public Promise<String> getDeviceCountry() {
        ThreadUtils.assertOnUiThread();
        return Promise.fulfilled(LocaleUtils.getDefaultCountryCode());
    }
}
