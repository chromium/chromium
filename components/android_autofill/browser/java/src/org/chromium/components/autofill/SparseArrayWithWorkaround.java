// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.util.SparseArray;
import android.view.autofill.VirtualViewFillInfo;

/**
 * This class works as a workaround for a bug (b/299532529) in the framework for autofill. DON'T use
 * elsewhere. It should be removed once we drop support for Android U.
 */
class SparseArrayWithWorkaround extends SparseArray<VirtualViewFillInfo> {

    @Override
    public int indexOfKey(int key) {
        return this.keyAt(key);
    }
}
