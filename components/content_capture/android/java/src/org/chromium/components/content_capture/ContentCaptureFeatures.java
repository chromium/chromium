// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.content_capture;

import org.jni_zero.NativeMethods;

import org.chromium.base.CommandLine;

/** The class to get if feature is enabled from native. */
public class ContentCaptureFeatures {
    private static final String FLAG = "dump-captured-content-to-logcat-for-testing";

    public static boolean isEnabled() {
        return ContentCaptureFeaturesJni.get().isEnabled();
    }

    public static boolean isDumpForTestingEnabled() {
        return CommandLine.getInstance().hasSwitch(FLAG);
    }

    public static boolean shouldTriggerContentCaptureForExperiment() {
        return ContentCaptureFeaturesJni.get().shouldTriggerContentCaptureForExperiment();
    }

    @NativeMethods
    public interface Natives {
        boolean isEnabled();

        boolean shouldTriggerContentCaptureForExperiment();
    }
}
