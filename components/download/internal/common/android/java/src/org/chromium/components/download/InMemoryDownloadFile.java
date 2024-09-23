// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.download;

import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.system.Os;

import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.Log;

import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.io.IOException;

/** Helper class for publishing download files to the public download collection. */
@JNINamespace("download")
public class InMemoryDownloadFile {
    private static final String TAG = "InMemoryDownload";
    private FileOutputStream mFos;
    private ParcelFileDescriptor mPfd;

    @CalledByNative
    @RequiresApi(Build.VERSION_CODES.R)
    private static InMemoryDownloadFile createFile(@JniType("std::string") String filename) {
        try {
            return new InMemoryDownloadFile(filename);
        } catch (Exception e) {
            return null;
        }
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private InMemoryDownloadFile(String filename) throws Exception {
        FileDescriptor mFd = Os.memfd_create(filename, 0);
        mPfd = ParcelFileDescriptor.dup(mFd);
        mFos = new FileOutputStream(mFd);
    }

    @CalledByNative
    private void writeData(@JniType("std::vector<uint8_t>") byte[] data) {
        try {
            mFos.write(data);
        } catch (Exception e) {
            Log.e(TAG, "failed to write data to file.", e);
        }
    }

    @CalledByNative
    private void finish() {
        try {
            if (mFos != null) {
                mFos.close();
                mFos = null;
            }
            // Detach the file descriptor, so native can use it even this
            // class is destroyed.
            if (mPfd != null) {
                mPfd.detachFd();
            }
        } catch (Exception e) {
            Log.e(TAG, "failed to close output stream.", e);
        }
    }

    @CalledByNative
    private int getFd() {
        return mPfd != null ? mPfd.getFd() : 0;
    }

    @CalledByNative
    private void destroy() {
        try {
            if (mFos != null) {
                mFos.close();
            }
            if (mPfd != null) {
                mPfd.close();
            }
        } catch (IOException e) {
            Log.e(TAG, "failed to close memory file.", e);
        }
    }
}
