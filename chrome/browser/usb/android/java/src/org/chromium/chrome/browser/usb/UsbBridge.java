// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usb;

import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;

/** Java access point for UsbBridge, allowing for querying USB state. */
@NullMarked
public class UsbBridge {
    public static boolean isWebContentsConnectedToUsbDevice(@Nullable WebContents webContents) {
        if (webContents == null) return false;
        return UsbBridgeJni.get().isWebContentsConnectedToUsbDevice(webContents);
    }

    @NativeMethods
    interface Natives {
        boolean isWebContentsConnectedToUsbDevice(WebContents webContents);
    }
}
