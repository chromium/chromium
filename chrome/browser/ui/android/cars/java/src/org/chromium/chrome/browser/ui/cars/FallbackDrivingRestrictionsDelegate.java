// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.cars;

import org.chromium.base.Callback;

/**
 * Instantiable version of {@link DrivingRestrictionsDelegate}, don't add anything to this class.
 * Downstream targets may provide a different implementation with
 * {@code @ServiceImpl(DrivingRestrictionsDelegateFactory.class)}.
 */
class FallbackDrivingRestrictionsDelegate extends DrivingRestrictionsDelegate {
    FallbackDrivingRestrictionsDelegate(Callback<Boolean> requiresDrivingOptimizationsCallback) {
        super(requiresDrivingOptimizationsCallback);
    }

    @Override
    void startMonitoring() {}

    @Override
    void stopMonitoring() {}
}
