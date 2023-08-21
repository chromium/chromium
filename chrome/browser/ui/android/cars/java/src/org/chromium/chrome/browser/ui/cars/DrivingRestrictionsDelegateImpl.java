// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.cars;

import org.chromium.base.Callback;

/**
 * Instantiable version of {@link DrivingRestrictionsDelegate}, don't add anything to this class.
 * Downstream targets may provide a different implementation. In GN, we specify that
 * {@link DrivingRestrictionsDelegate} is compiled separately from its implementation; other
 * projects may specify a different DrivingRestrictionsDelegate via GN.
 */
class DrivingRestrictionsDelegateImpl extends DrivingRestrictionsDelegate {
    DrivingRestrictionsDelegateImpl(Callback<Boolean> requiresDrivingOptimizationsCallback) {
        super(requiresDrivingOptimizationsCallback);
    }
}
