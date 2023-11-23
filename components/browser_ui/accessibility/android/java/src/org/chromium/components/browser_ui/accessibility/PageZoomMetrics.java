// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import androidx.annotation.NonNull;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;

/** Centralizes metrics data collection for Page Zoom. */
@JNINamespace("browser_ui")
public class PageZoomMetrics {
    /**
     * Logs new zoom level UKM for the given web contents.
     * Recorded on slider dismissal if the user chose a new value.
     * @param webContents   WebContents for which to log value
     * @param newZoomLevel  double - new zoom level
     */
    public static void logZoomLevelUKM(@NonNull WebContents webContents, double newZoomLevel) {
        PageZoomMetricsJni.get().logZoomLevelUKM(webContents, newZoomLevel);
    }

    @NativeMethods
    public interface Natives {
        void logZoomLevelUKM(WebContents webContents, double value);
    }
}
