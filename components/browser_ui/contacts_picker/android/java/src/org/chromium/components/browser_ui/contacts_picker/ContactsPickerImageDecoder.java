// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import android.graphics.Bitmap;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Utility class for safely decoding contact photo images using a sandboxed utility service. */
@JNINamespace("browser_ui")
@NullMarked
public class ContactsPickerImageDecoder {
    public static final int MAX_IMAGE_SIZE_BYTES = 1024 * 1024; // 1 MiB

    private ContactsPickerImageDecoder() {}

    /**
     * Decodes raw image data in an isolated utility process.
     *
     * @param data The raw compressed image data.
     * @param desiredSize The desired width/height in pixels, or 0 if unscaled.
     * @param callback The callback to receive the decoded Bitmap (or null on failure).
     */
    public static void decodeImage(
            byte @Nullable [] data, int desiredSize, Callback<@Nullable Bitmap> callback) {
        ThreadUtils.assertOnUiThread();
        if (data == null || data.length == 0 || data.length > MAX_IMAGE_SIZE_BYTES) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, callback.bind(null));
            return;
        }
        ContactsPickerImageDecoderJni.get().decodeImage(data, desiredSize, callback);
    }

    @NativeMethods
    interface Natives {
        void decodeImage(byte[] data, int desiredSize, Callback<@Nullable Bitmap> callback);
    }
}
