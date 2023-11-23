// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import android.util.Log;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import java.io.IOException;
import java.io.InputStream;

/** Utility methods for calling InputStream methods. These take care of exception handling. */
@JNINamespace("embedder_support")
class InputStreamUtil {
    private static final String LOGTAG = "InputStreamUtil";
    // The InputStream APIs return -1 in some cases. In order to convey the extra information that
    // the call had failed due to an exception being thrown we simply map all negative return values
    // from the original calls to -1 and make -2 mean that an exception has been thrown.
    private static final int CALL_FAILED_STATUS = -1;
    private static final int EXCEPTION_THROWN_STATUS = -2;

    private static String logMessage(String method) {
        return "Got exception when calling "
                + method
                + "() on an InputStream returned from "
                + "shouldInterceptRequest. This will cause the related request to fail.";
    }

    @CalledByNative
    public static void close(InputStream stream) {
        try {
            stream.close();
        } catch (IOException e) {
            Log.e(LOGTAG, logMessage("close"), e);
        }
    }

    @CalledByNative
    public static int available(InputStream stream) {
        try {
            return Math.max(CALL_FAILED_STATUS, stream.available());
        } catch (IOException e) {
            Log.e(LOGTAG, logMessage("available"), e);
            return EXCEPTION_THROWN_STATUS;
        }
    }

    @CalledByNative
    public static int read(InputStream stream, byte[] b, int off, int len) {
        try {
            return Math.max(CALL_FAILED_STATUS, stream.read(b, off, len));
        } catch (IOException e) {
            Log.e(LOGTAG, logMessage("read"), e);
            return EXCEPTION_THROWN_STATUS;
        }
    }

    @CalledByNative
    public static long skip(InputStream stream, long n) {
        try {
            return Math.max(CALL_FAILED_STATUS, stream.skip(n));
        } catch (IOException e) {
            Log.e(LOGTAG, logMessage("skip"), e);
            return EXCEPTION_THROWN_STATUS;
        }
    }
}
