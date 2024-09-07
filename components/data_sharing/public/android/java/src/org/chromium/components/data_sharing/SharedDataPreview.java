// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.JNINamespace;

import java.util.List;

/** Preview of shared data. */
@JNINamespace("data_sharing")
public class SharedDataPreview {
    public final List<SharedEntity> sharedEntities;

    public SharedDataPreview(SharedEntity[] sharedEntities) {
        this.sharedEntities = sharedEntities == null ? null : List.of(sharedEntities);
    }
}
