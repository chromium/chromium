// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;

import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;

/**
 * This class groups all the {@link AccountInfoService} instance manipulation
 * methods in one place.
 */
public final class AccountInfoServiceProvider {
    private static @Nullable Promise<AccountInfoService> sInstancePromise;

    /** Initializes the singleton {@link AccountInfoService} instance. */
    @MainThread
    public static void init(IdentityManager identityManager) {
        if (sInstancePromise != null && sInstancePromise.isFulfilled()) {
            return;
        }
        final AccountInfoService service = new AccountInfoServiceImpl(identityManager);
        if (sInstancePromise == null) {
            sInstancePromise = Promise.fulfilled(service);
        } else {
            sInstancePromise.fulfill(service);
        }
    }

    /**
     * Gets the singleton {@link AccountInfoService} instance.
     *
     * This method must be invoked after {@link AccountInfoService} is initialized.
     */
    @MainThread
    public static AccountInfoService get() {
        ThreadUtils.assertOnUiThread();
        final Promise<AccountInfoService> promise = getPromise();
        if (!promise.isFulfilled()) {
            throw new RuntimeException("The AccountInfoService is not yet initialized!");
        }
        return promise.getResult();
    }

    /**
     * Gets the {@link Promise} of the singleton {@link AccountInfoService} instance.
     *
     * This method can be invoked before {@link AccountInfoService} is initialized.
     */
    @MainThread
    public static Promise<AccountInfoService> getPromise() {
        ThreadUtils.assertOnUiThread();
        if (sInstancePromise == null) {
            sInstancePromise = new Promise<>();
        }
        return sInstancePromise;
    }

    @MainThread
    public static void setInstanceForTests(AccountInfoService accountInfoService) {
        ThreadUtils.assertOnUiThread();
        sInstancePromise = Promise.fulfilled(accountInfoService);
    }

    public static void resetForTests() {
        sInstancePromise = null;
    }

    private AccountInfoServiceProvider() {}
}
