// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import static org.mockito.Mockito.doReturn;

import android.os.Handler;
import android.os.Looper;

import org.jni_zero.CalledByNative;
import org.mockito.Mockito;

import org.chromium.base.Promise;
import org.chromium.base.test.util.LooperUtils;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.CoreAccountInfo;

import java.lang.reflect.InvocationTargetException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Util class to support {@link org.chromium.components.signin.AccountCapabilitiesFetcher} native
 * tests.
 */
final class AccountCapabilitiesFetcherTestUtil {
    private AccountManagerFacade mMockFacade;
    private CoreAccountInfo mExpectedAccount;
    private Promise<AccountCapabilities> mCapabilitiesPromise;

    /** Stubs AccountManagerFacade for native tests. */
    @CalledByNative
    public AccountCapabilitiesFetcherTestUtil() {
        mMockFacade = Mockito.mock(AccountManagerFacade.class);
        AccountManagerFacadeProvider.setInstanceForTests(mMockFacade);
    }

    /** Restores the global state after the test completes. */
    @CalledByNative
    public void destroy() {
        mMockFacade = null;
    }

    /**
     * Sets expectation that {@link AccountManagerFacade#getAccountCapabilities} should be called
     * with {@code accountInfo}.
     */
    @CalledByNative
    public void expectAccount(CoreAccountInfo accountInfo) {
        // Only one account at a time is supported.
        assert mCapabilitiesPromise == null;
        assert mExpectedAccount == null;

        mExpectedAccount = accountInfo;
        mCapabilitiesPromise = new Promise<>();
        doReturn(mCapabilitiesPromise).when(mMockFacade).getAccountCapabilities(accountInfo);
    }

    /**
     * Fulfills pending account capabilities request. Must be called after after {@link
     * #expectAccount} has been called with the same {@code accountInfo} parameter.
     */
    @CalledByNative
    public void returnCapabilities(CoreAccountInfo accountInfo, AccountCapabilities capabilities) {
        assert mCapabilitiesPromise != null;
        assert mExpectedAccount != null;
        assert mExpectedAccount.equals(accountInfo);

        mCapabilitiesPromise.fulfill(capabilities);
        // `Promise` posts callback tasks on Android Looper which is not integrated with native
        // RunLoop in NativeTest. Run these tasks synchronously now.
        // TODO(crbug.com/40723709): remove this hack once Promise uses PostTask.
        runLooperTasks();

        mCapabilitiesPromise = null;
        mExpectedAccount = null;
    }

    /**
     * Runs all tasks that are currently posted on the {@link Looper}'s message queue on the current
     * thread.
     */
    private static void runLooperTasks() {
        AtomicBoolean called = new AtomicBoolean(false);
        new Handler(Looper.myLooper())
                .post(
                        () -> {
                            called.set(true);
                        });

        do {
            try {
                LooperUtils.runSingleNestedLooperTask();
            } catch (IllegalArgumentException
                    | IllegalAccessException
                    | SecurityException
                    | InvocationTargetException e) {
                throw new RuntimeException(e);
            }
        } while (!called.get());
    }
}
