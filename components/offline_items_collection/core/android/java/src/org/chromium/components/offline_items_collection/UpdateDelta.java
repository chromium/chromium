// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.offline_items_collection;

/**
 * This class is the Java counterpart to the C++ UpdateDelta
 * (components/offline_items_collection/core/state_change.h) class.
 *
 * For all member variable descriptions see the C++ class.
 */
public class UpdateDelta {
    public boolean stateChanged;
    public boolean visualsChanged;

    /** Constructor. Keep the default values same as C++. */
    public UpdateDelta() {
        stateChanged = true;
        visualsChanged = false;
    }
}
