// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import androidx.annotation.MainThread;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Created by native code to get status of {@link AccountManagerFacade#isUpdatePending()} and
 * notifications when it changes.
 */
public class ConsistencyCookieManager
        implements ObservableValue.Observer, AccountTrackerService.OnSystemAccountsSeededListener {
    private final long mNativeConsistencyCookieManager;
    private final AccountTrackerService mAccountTrackerService;
    private final AccountManagerFacade mAccountManagerFacade;
    private final SigninActivityMonitor mSigninActivityMonitor;
    private boolean mIsUpdatePending;

    private ConsistencyCookieManager(
            long nativeConsistencyCookieManager, AccountTrackerService accountTrackerService) {
        ThreadUtils.assertOnUiThread();
        mAccountTrackerService = accountTrackerService;
        mNativeConsistencyCookieManager = nativeConsistencyCookieManager;
        mAccountManagerFacade = AccountManagerFacade.get();
        mSigninActivityMonitor = SigninActivityMonitor.get();

        // For now, this class relies on the order of notifications sent by AccountManagerFacade and
        // AccountTrackerService. Whenever the account list changes, this class will get
        // notification from AccountManagerFacade.isUpdatePending(). This will change
        // mIsUpdatePending to true. By the time AccountManagerFacade finishes updating account list
        // and sets AccountManagerFacade.isUpdatePending() to false, AccountTrackerService should
        // have already invalidate account seed status, so mIsUpdatePending will stay false until
        // accounts are seeded to the native AccountTrackerService.
        // TODO(https://crbug.com/831257): Simplify this after seeding is reimplemented.
        mAccountTrackerService.addSystemAccountsSeededListener(this);
        mAccountManagerFacade.isUpdatePending().addObserver(this);
        mSigninActivityMonitor.hasOngoingActivity().addObserver(this);

        mIsUpdatePending = calculateIsUpdatePending();
    }

    @Override
    public void onSystemAccountsSeedingComplete() {
        onValueChanged();
    }

    @Override
    public void onValueChanged() {
        boolean state = calculateIsUpdatePending();

        if (mIsUpdatePending == state) return;
        mIsUpdatePending = state;
        ConsistencyCookieManagerJni.get().onIsUpdatePendingChanged(mNativeConsistencyCookieManager);
    }

    private boolean calculateIsUpdatePending() {
        return mAccountManagerFacade.isUpdatePending().get()
                || mSigninActivityMonitor.hasOngoingActivity().get()
                || !mAccountTrackerService.areSystemAccountsSeeded();
    }

    @CalledByNative
    @MainThread
    private static ConsistencyCookieManager create(
            long nativeConsistencyCookieManager, AccountTrackerService accountTrackerService) {
        return new ConsistencyCookieManager(nativeConsistencyCookieManager, accountTrackerService);
    }

    @CalledByNative
    @MainThread
    private void destroy() {
        ThreadUtils.assertOnUiThread();
        mAccountTrackerService.removeSystemAccountsSeededListener(this);
        mSigninActivityMonitor.hasOngoingActivity().removeObserver(this);
        mAccountManagerFacade.isUpdatePending().removeObserver(this);
    }

    @CalledByNative
    @MainThread
    private boolean getIsUpdatePending() {
        ThreadUtils.assertOnUiThread();
        return mIsUpdatePending;
    }

    @JNINamespace("signin")
    @NativeMethods
    interface Natives {
        void onIsUpdatePendingChanged(long nativeConsistencyCookieManagerAndroid);
    }
}
