// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.cars;

import org.chromium.base.Callback;

/** Base class that exposes methods for starting and stopping monitoring. */
public abstract class DrivingRestrictionsDelegate {
    private final Callback<Boolean> mRequiresDrivingOptimizationsCallback;

    DrivingRestrictionsDelegate(Callback<Boolean> requiresDrivingOptimizationsCallback) {
        mRequiresDrivingOptimizationsCallback = requiresDrivingOptimizationsCallback;
    }

    abstract void startMonitoring();

    abstract void stopMonitoring();

    protected void notifyCallback(boolean requiresOptimization) {
        mRequiresDrivingOptimizationsCallback.onResult(requiresOptimization);
    }
}
