// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.cars;

import org.chromium.base.Callback;

/** Instantiates a {@link DrivingRestrictionsDelegate}. */
public interface DrivingRestrictionsDelegateFactory {
    DrivingRestrictionsDelegate create(Callback<Boolean> requiresDrivingOptimizationsCallback);
}
