// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * Wrapper of native CastMetricsHelper.
 */
@JNINamespace("chromecast::shell")
public final class CastMetricsHelper {
    public static void logMediaPlay() {
        CastMetricsHelperJni.get().logMediaPlay();
    }

    public static void logMediaPause() {
        CastMetricsHelperJni.get().logMediaPause();
    }

    @NativeMethods
    interface Natives {
        void logMediaPlay();
        void logMediaPause();
    }
}
