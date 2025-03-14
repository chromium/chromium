// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.input;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

@JNINamespace("input")
@NullMarked
public class InputUtils {

    @Nullable private static Boolean sIsTransferInputToVizSupported;

    public static boolean isTransferInputToVizSupported() {
        // Assert native has been initialized. This will be optimized out in Release builds.
        InputUtilsJni.get();
        // Assert feature list has been initialized on native side since
        // `isTransferInputToVizSupported` on native side queries the list.
        assert FeatureList.isNativeInitialized();
        if (sIsTransferInputToVizSupported == null) {
            sIsTransferInputToVizSupported = InputUtilsJni.get().isTransferInputToVizSupported();
        }
        return sIsTransferInputToVizSupported;
    }

    @NativeMethods
    interface Natives {
        boolean isTransferInputToVizSupported();
    }
}
