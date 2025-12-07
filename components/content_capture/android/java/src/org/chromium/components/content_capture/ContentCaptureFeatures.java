// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.content_capture;

import org.jni_zero.NativeMethods;

import org.chromium.base.CommandLine;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** The class to get if feature is enabled from native. */
@NullMarked
public class ContentCaptureFeatures {
    private static final String FLAG = "dump-captured-content-to-logcat-for-testing";
    private static @Nullable Boolean sEnableDebugLogging;

    public static boolean isEnabled() {
        return ContentCaptureFeaturesJni.get().isEnabled();
    }

    public static boolean isDumpForTestingEnabled() {
        if (sEnableDebugLogging == null) {
            sEnableDebugLogging = CommandLine.getInstance().hasSwitch(FLAG);
        }
        return sEnableDebugLogging;
    }

    @NativeMethods
    public interface Natives {
        boolean isEnabled();
    }
}
