// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Preview of shared data. */
@JNINamespace("data_sharing")
@NullMarked
public class SharedDataPreview {
    public final SharedTabGroupPreview sharedTabGroupPreview;
    // Remove this when downstream change lands.
    public final @Nullable SharedTabGroupPreview sharedEntities = null;

    public SharedDataPreview(SharedTabGroupPreview sharedTabGroupPreview) {
        this.sharedTabGroupPreview = sharedTabGroupPreview;
    }
}
