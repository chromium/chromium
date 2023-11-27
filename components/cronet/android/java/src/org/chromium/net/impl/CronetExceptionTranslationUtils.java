// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import android.net.http.HttpException;

import androidx.annotation.RequiresExtension;

import org.chromium.net.CronetException;

@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
class CronetExceptionTranslationUtils {
    @SuppressWarnings("unchecked")
    public static <T, E extends Exception> T executeTranslatingCronetExceptions(
            CronetWork<T, E> work, Class<E> nonCronetException) throws CronetException, E {
        try {
            return work.run();
        } catch (Exception e) {
            if (isUncheckedAndroidCronetException(e)) {
                throw translateUncheckedAndroidCronetException(e);
            } else if (isCheckedAndroidCronetException(e)) {
                throw translateCheckedAndroidCronetException(e);
            } else if (nonCronetException.isInstance(e)) {
                throw (E) e;
            } else {
                throw e;
            }
        }
    }

    public static boolean isUncheckedAndroidCronetException(Exception e) {
        return (e instanceof android.net.http.InlineExecutionProhibitedException);
    }

    public static boolean isCheckedAndroidCronetException(Exception e) {
        return (e instanceof HttpException);
    }

    public static RuntimeException translateUncheckedAndroidCronetException(Exception e) {
        if (!isUncheckedAndroidCronetException(e)) {
            throw new IllegalArgumentException("Not an Android Cronet exception", e);
        }

        if (e instanceof android.net.http.InlineExecutionProhibitedException) {
            // InlineExecutionProhibitedException is final so we can't have our own flavor
            org.chromium.net.InlineExecutionProhibitedException wrappedException =
                    new org.chromium.net.InlineExecutionProhibitedException();
            wrappedException.initCause(e);
            return wrappedException;
        }

        throw new UnsupportedOperationException("Unchecked exception translation discrepancy", e);
    }

    public static CronetException translateCheckedAndroidCronetException(Exception e) {
        if (!isCheckedAndroidCronetException(e)) {
            throw new IllegalArgumentException("Not an Android Cronet exception", e);
        }

        if (e instanceof android.net.http.QuicException) {
            return new AndroidQuicExceptionWrapper((android.net.http.QuicException) e);
        } else if (e instanceof android.net.http.NetworkException) {
            return new AndroidNetworkExceptionWrapper((android.net.http.NetworkException) e);
        } else if (e instanceof android.net.http.CallbackException) {
            return new AndroidCallbackExceptionWrapper((android.net.http.CallbackException) e);
        } else if (e instanceof HttpException) {
            return new AndroidHttpExceptionWrapper((HttpException) e);
        }

        throw new UnsupportedOperationException("Checked exception translation discrepancy", e);
    }

    interface CronetWork<T, E extends Exception> {
        T run() throws E;
    }

    private CronetExceptionTranslationUtils() {}
}
