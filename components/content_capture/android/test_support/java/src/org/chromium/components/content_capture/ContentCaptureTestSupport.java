// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.content_capture;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;

/** This is the test support class to help setup various test conditions. */
@JNINamespace("content_capture")
public class ContentCaptureTestSupport {
    public static void disableGetFaviconFromWebContents() {
        ContentCaptureTestSupportJni.get().disableGetFaviconFromWebContents();
    }

    public static void simulateDidUpdateFaviconURL(WebContents webContents, String faviconJson) {
        ContentCaptureTestSupportJni.get().simulateDidUpdateFaviconURL(webContents, faviconJson);
    }

    @NativeMethods
    interface Natives {
        void disableGetFaviconFromWebContents();

        void simulateDidUpdateFaviconURL(WebContents webContents, String faviconJson);
    }
}
