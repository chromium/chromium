// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.signin;

import androidx.annotation.AnyThread;
import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

import java.util.concurrent.atomic.AtomicReference;

/**
 * AccountManagerFacadeProvider is intended to group all the
 * AccountManagerFacade instance manipulation methods in one place.
 */
public class AccountManagerFacadeProvider {
    private static final String TAG = "AccManagerProvider";
    private static final AtomicReference<AccountManagerFacade> sAtomicInstance =
            new AtomicReference<>();
    private static AccountManagerFacade sInstance;
    private static AccountManagerFacade sTestingInstance;

    private AccountManagerFacadeProvider() {}

    /**
     * Sets AccountManagerFacade singleton instance. Can only be called once.
     * Tests can override the instance with {@link #setInstanceForTests}.
     *
     */
    @MainThread
    public static void setInstance(AccountManagerFacade accountManagerFacade) {
        ThreadUtils.assertOnUiThread();
        if (sInstance != null) {
            throw new IllegalStateException("AccountManagerFacade is already initialized!");
        }
        sInstance = accountManagerFacade;
        if (sTestingInstance != null) return;
        sAtomicInstance.set(sInstance);
    }

    /** Sets the test instance. */
    @VisibleForTesting
    @AnyThread
    public static void setInstanceForTests(AccountManagerFacade accountManagerFacade) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sTestingInstance = accountManagerFacade;
                    sAtomicInstance.set(sTestingInstance);
                });
    }

    /** Resets the test instance set with {@link #setInstanceForTests}. */
    @VisibleForTesting
    @AnyThread
    public static void resetInstanceForTests() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sTestingInstance = null;
                    sAtomicInstance.set(sInstance);
                    Log.d(TAG, "reset AccountManagerFacade test instance");
                });
    }

    /**
     * Singleton instance getter. Singleton must be initialized before calling this by
     * {@link #setInstance} or {@link #setInstanceForTests}.
     *
     * @return a singleton instance
     */
    @AnyThread
    @CalledByNative
    public static AccountManagerFacade getInstance() {
        AccountManagerFacade instance = sAtomicInstance.get();
        if (instance == null) {
            throw new IllegalStateException("AccountManagerFacade is not yet initialized!");
        }
        return instance;
    }
}
