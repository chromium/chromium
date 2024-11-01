// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.JNINamespace;

/** Preview of shared data. */
@JNINamespace("data_sharing")
public class SharedDataPreview {
    public final SharedTabGroupPreview sharedTabGroupPreview;
    // Remove this when downstream change lands.
    public final SharedTabGroupPreview sharedEntities = null;

    public SharedDataPreview(SharedTabGroupPreview sharedTabGroupPreview) {
        this.sharedTabGroupPreview = sharedTabGroupPreview;
    }
}
