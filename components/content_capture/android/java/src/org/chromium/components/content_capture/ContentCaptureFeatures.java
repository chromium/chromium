// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.content_capture;

import org.chromium.base.CommandLine;
import org.chromium.base.annotations.NativeMethods;

/**
 * The class to get if feature is enabled from native.
 */
public class ContentCaptureFeatures {
    private static final String FLAG = "dump-captured-content-to-logcat-for-testing";

    public static boolean isEnabled() {
        return ContentCaptureFeaturesJni.get().isEnabled();
    }

    public static boolean isDumpForTestingEnabled() {
        return CommandLine.getInstance().hasSwitch(FLAG);
    }

    @NativeMethods
    interface Natives {
        boolean isEnabled();
    }
}
