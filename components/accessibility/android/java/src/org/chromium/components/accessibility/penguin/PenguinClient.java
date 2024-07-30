// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.accessibility.penguin;

import android.graphics.Bitmap;
import android.graphics.Rect;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;

/** Java-side hook for {@link penguin_client.cc}. */
@JNINamespace("penguin")
public class PenguinClient {

    public PenguinClient(BrowserContextHandle browserContext) {}

    public void performAPICall(
            Bitmap image, String textInput, Runnable callback, boolean includeFullResponse) {}

    public void performAPICall(
            String imageData, String textInput, Runnable callback, boolean includeFullResponse) {}

    public void performAPICall(
            WebContents webContents,
            String textInput,
            Runnable callback,
            boolean includeFullResponse) {}

    public void performAPICall(
            WebContents webContents,
            String textInput,
            Runnable callback,
            Rect sourceRect,
            boolean includeFullResponse) {}

    public void performAPICall(String textInput, Runnable callback, boolean includeFullResponse) {}

    @NativeMethods
    interface Natives {

        long create(BrowserContextHandle browserContext);

        void performAPICall_var1(
                long nativePenguinClient,
                String imageData,
                String textInput,
                Runnable callback,
                boolean includeFullResponse);

        void performAPICall_var2(
                long nativePenguinClient,
                WebContents webContents,
                String textInput,
                Runnable callback,
                boolean includeFullResponse);

        void performAPICall_var3(
                long nativePenguinClient,
                WebContents webContents,
                String textInput,
                Runnable callback,
                Rect sourceRect,
                boolean includeFullResponse);

        void performAPICall_var4(
                long nativePenguinClient,
                String textInput,
                Runnable callback,
                boolean includeFullResponse);
    }
}
