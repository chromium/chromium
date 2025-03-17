// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/** Preview about a shared tab group. */
@JNINamespace("data_sharing")
@NullMarked
public class SharedTabGroupPreview {
    public final String title;
    public final @Nullable List<TabPreview> tabs;

    private static final String TAG = "SharedEntity";

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public SharedTabGroupPreview(String title, @Nullable List<TabPreview> tabs) {
        this.title = title;
        this.tabs = tabs;
    }

    @CalledByNative
    private static SharedTabGroupPreview createSharedTabGroupPreview(
            String title, TabPreview @Nullable [] tabs) {
        return new SharedTabGroupPreview(title, tabs == null ? null : List.of(tabs));
    }
}
