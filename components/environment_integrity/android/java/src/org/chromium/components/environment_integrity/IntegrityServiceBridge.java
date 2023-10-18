// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.environment_integrity;

import androidx.annotation.NonNull;
import androidx.core.os.ExecutorCompat;

import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.components.environment_integrity.enums.IntegrityResponse;

/**
 * Interface to the Play EnvironmentIntegrity API.
 *
 * This class dynamically instantiates a subclass which can be swapped out by the build system.
 */
@JNINamespace("environment_integrity")
public abstract class IntegrityServiceBridge {
    private static IntegrityServiceBridgeDelegate sDelegate;
    private static boolean sBindAppIdentity;

    /**
     * Set whether new handles should bind the app identity in tokens they issue.
     * This defaults to {@code false}.
     * @param bindAppIdentity {@code true} if the app identity (package name) should be included in
     *         issued tokens.
     */
    public static void setBindAppIdentity(boolean bindAppIdentity) {
        ThreadUtils.checkUiThread();
        sBindAppIdentity = bindAppIdentity;
    }

    /**
     * Check if calling {@link #createHandle(long, int)} or {@link #getIntegrityToken(long, long,
     * byte[], int)} can be expected to succeed.
     * @return {@code false} if other methods are guaranteed to fail.
     */
    @CalledByNative
    public static boolean isIntegrityAvailable() {
        ThreadUtils.checkUiThread();
        return IntegrityServiceBridge.getDelegate().canUseGms();
    }

    /**
     * Create a new handle to issue integrity tokens.
     *
     * Will respond by calling {@link Natives#onCreateHandleResult(long, int, long, String)}.
     *
     * @param callbackId pointer to native callback.
     * @param timeoutMilliseconds timeout before aborting
     */
    @CalledByNative
    public static void createHandle(final long callbackId, int timeoutMilliseconds) {
        ThreadUtils.checkUiThread();

        final ListenableFuture<Long> future =
                IntegrityServiceBridge.getDelegate().createEnvironmentIntegrityHandle(
                        sBindAppIdentity, timeoutMilliseconds);

        Futures.addCallback(future, new FutureCallback<>() {
            @Override
            public void onSuccess(@NonNull Long handle) {
                IntegrityServiceBridgeJni.get().onCreateHandleResult(
                        callbackId, IntegrityResponse.SUCCESS, handle, null);
            }

            @Override
            public void onFailure(@NonNull Throwable t) {
                if (t instanceof IntegrityException) {
                    IntegrityException ex = (IntegrityException) t;
                    IntegrityServiceBridgeJni.get().onCreateHandleResult(
                            callbackId, ex.getErrorCode(), 0L, ex.getMessage());
                } else {
                    IntegrityServiceBridgeJni.get().onCreateHandleResult(
                            callbackId, IntegrityResponse.UNKNOWN_ERROR, 0L, "Unknown Error.");
                }
            }
        }, ExecutorCompat.create(ThreadUtils.getUiThreadHandler()));
    }

    /**
     * Request a new integrity token bound to the provided {@code contentBinding}.
     *
     * Will respond by calling {@link Natives#onGetIntegrityTokenResult(long, int, byte[], String)}.
     *
     * @param callbackId pointer to native callback.
     * @param handle integrity handle for this app.
     * @param contentBinding hashed content binding to sign.
     * @param timeoutMilliseconds timeout before aborting
     */
    @CalledByNative
    public static void getIntegrityToken(
            final long callbackId, long handle, byte[] contentBinding, int timeoutMilliseconds) {
        ThreadUtils.checkUiThread();

        final ListenableFuture<byte[]> future =
                IntegrityServiceBridge.getDelegate().getEnvironmentIntegrityToken(
                        handle, contentBinding, timeoutMilliseconds);

        Futures.addCallback(future, new FutureCallback<>() {
            @Override
            public void onSuccess(@NonNull byte[] token) {
                IntegrityServiceBridgeJni.get().onGetIntegrityTokenResult(
                        callbackId, IntegrityResponse.SUCCESS, token, null);
            }

            @Override
            public void onFailure(@NonNull Throwable t) {
                if (t instanceof IntegrityException) {
                    IntegrityException ex = (IntegrityException) t;
                    IntegrityServiceBridgeJni.get().onGetIntegrityTokenResult(
                            callbackId, ex.getErrorCode(), null, ex.getMessage());
                } else {
                    IntegrityServiceBridgeJni.get().onGetIntegrityTokenResult(
                            callbackId, IntegrityResponse.UNKNOWN_ERROR, null, "Unknown Error.");
                }
            }
        }, ExecutorCompat.create(ThreadUtils.getUiThreadHandler()));
    }

    private static IntegrityServiceBridgeDelegate getDelegate() {
        ThreadUtils.checkUiThread();
        if (sDelegate == null) {
            sDelegate = new IntegrityServiceBridgeDelegateImpl();
        }
        return sDelegate;
    }

    /**
     * Set the Delegate implementation to use. Will automatically reset after test.
     * @param delegateForTesting Delegate implementation to use during test.
     */
    public static void setDelegateForTesting(IntegrityServiceBridgeDelegate delegateForTesting) {
        ThreadUtils.checkUiThread();
        sDelegate = delegateForTesting;
        ResettersForTesting.register(
                ()
                        -> ThreadUtils.runOnUiThreadBlockingNoException(
                                () -> IntegrityServiceBridge.sDelegate = null));
    }

    @NativeMethods
    interface Natives {
        void onCreateHandleResult(
                long callbackId, @IntegrityResponse int responseCode, long handle, String errorMsg);

        void onGetIntegrityTokenResult(long callbackId, @IntegrityResponse int responseCode,
                byte[] token, String errorMsg);
    }
}
