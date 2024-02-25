// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.os.SystemClock;
import android.util.Pair;

import org.jni_zero.NativeMethods;

import org.chromium.base.Log;

import java.io.FileDescriptor;
import java.io.IOException;

/**
 * A helper to accept requests to take image file contents and decode them. As this is intended to
 * be run in a separate, sandboxed process, it also requires calling code to initialize the sandbox.
 */
public class ImageDecoder extends IDecoderService.Stub {
    // The keys for the bundle when passing data to and from this service.
    public static final String KEY_FILE_DESCRIPTOR = "file_descriptor";
    public static final String KEY_FILE_PATH = "file_path";
    public static final String KEY_IMAGE_BITMAP = "image_bitmap";
    public static final String KEY_WIDTH = "width";
    public static final String KEY_RATIO = "ratio";
    public static final String KEY_FULL_WIDTH = "full_width";
    public static final String KEY_SUCCESS = "success";
    public static final String KEY_DECODE_TIME = "decode_time";

    // A tag for logging error messages.
    private static final String TAG = "ImageDecoder";

    // Whether the native library and the sandbox have been initialized.
    private boolean mSandboxInitialized;

    /** Initializes the seccomp-bpf sandbox when it's supported by the device. */
    public void initializeSandbox() {
        ImageDecoderJni.get().initializePhotoPickerSandbox();
        mSandboxInitialized = true;
    }

    @Override
    public void decodeImage(Bundle payload, IDecoderServiceCallback callback) {
        Bundle bundle = null;
        String filePath = "";
        int width = 0;
        boolean fullWidth = false;
        try {
            filePath = payload.getString(KEY_FILE_PATH);
            ParcelFileDescriptor pfd = payload.getParcelable(KEY_FILE_DESCRIPTOR);
            width = payload.getInt(KEY_WIDTH);
            fullWidth = payload.getBoolean(KEY_FULL_WIDTH);

            // Setup a minimum viable response to parent process. Will be fleshed out
            // further below.
            bundle = new Bundle();
            bundle.putString(KEY_FILE_PATH, filePath);
            bundle.putBoolean(KEY_SUCCESS, false);

            if (!mSandboxInitialized) {
                Log.e(TAG, "Decode failed " + filePath + " (width: " + width + "): no sandbox");
                sendReply(callback, bundle); // Sends SUCCESS == false;
                return;
            }

            FileDescriptor fd = pfd.getFileDescriptor();

            long begin = SystemClock.elapsedRealtime();
            Pair<Bitmap, Float> decodedBitmap =
                    BitmapUtils.decodeBitmapFromFileDescriptor(fd, width, fullWidth);
            long decodeTime = SystemClock.elapsedRealtime() - begin;

            try {
                pfd.close();
            } catch (IOException e) {
                Log.e(TAG, "Closing failed " + filePath + " (width: " + width + ") " + e);
            }

            Bitmap bitmap = decodedBitmap != null ? decodedBitmap.first : null;
            if (bitmap == null) {
                Log.e(TAG, "Decode failed " + filePath + " (width: " + width + ")");
                sendReply(callback, bundle); // Sends SUCCESS == false;
                return;
            }

            // The most widely supported, easiest, and reasonably efficient method is to
            // decode to an immutable bitmap and just return the bitmap over binder. It
            // will internally memcpy itself to ashmem and then just send over the file
            // descriptor. In the receiving process it will just leave the bitmap on
            // ashmem since it's immutable and carry on.
            bundle.putParcelable(KEY_IMAGE_BITMAP, bitmap);
            bundle.putFloat(KEY_RATIO, decodedBitmap.second);
            bundle.putBoolean(KEY_SUCCESS, true);
            bundle.putLong(KEY_DECODE_TIME, decodeTime);
            bundle.putBoolean(KEY_FULL_WIDTH, payload.getBoolean(KEY_FULL_WIDTH));
            sendReply(callback, bundle);
            bitmap.recycle();
        } catch (Exception e) {
            // This service has no UI and maintains no state so if it crashes on
            // decoding a photo, it is better UX to eat the exception instead of showing
            // a crash dialog and discarding other requests that have already been sent.
            Log.e(
                    TAG,
                    "Unexpected error during decoding "
                            + filePath
                            + " (width: "
                            + width
                            + ") "
                            + e);

            if (bundle != null) sendReply(callback, bundle);
        }
    }

    private void sendReply(IDecoderServiceCallback callback, Bundle bundle) {
        try {
            callback.onDecodeImageDone(bundle);
        } catch (RemoteException remoteException) {
            Log.e(TAG, "Remote error while replying: " + remoteException);
        }
    }

    @NativeMethods
    interface Natives {
        void initializePhotoPickerSandbox();
    }
}
