// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.invalidation;

import android.accounts.Account;
import android.content.Intent;
import android.os.Build;

import androidx.annotation.VisibleForTesting;

import com.google.protos.ipc.invalidation.Types;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.sync.notifier.InvalidationClientNameProvider;
import org.chromium.components.sync.notifier.InvalidationIntentProtocol;
import org.chromium.components.sync.notifier.InvalidationPreferences;

/**
 * Wrapper for invalidations::InvalidationServiceAndroid.
 *
 * Serves as the bridge between Java and C++ for the invalidations component.
 */
@JNINamespace("invalidation")
public class InvalidationService {
    private final long mNativeInvalidationServiceAndroid;

    private static final String TAG = "invalidation";

    private InvalidationService(long nativeInvalidationServiceAndroid) {
        mNativeInvalidationServiceAndroid = nativeInvalidationServiceAndroid;
    }

    public void notifyInvalidationToNativeChrome(
            int objectSource, String objectId, long version, String payload) {
        ThreadUtils.assertOnUiThread();
        InvalidationServiceJni.get().invalidate(mNativeInvalidationServiceAndroid,
                InvalidationService.this, objectSource, objectId, version, payload);
    }

    public void requestSyncFromNativeChromeForAllTypes() {
        notifyInvalidationToNativeChrome(Types.ObjectSource.CHROME_SYNC, null, 0L, null);
    }

    @CalledByNative
    private static InvalidationService create(long nativeInvalidationServiceAndroid) {
        ThreadUtils.assertOnUiThread();
        return new InvalidationService(nativeInvalidationServiceAndroid);
    }

    /**
     * Sets object ids for which the client should register for notification. This is intended for
     * registering non-Sync types; Sync types are registered with {@code setRegisteredTypes}.
     *
     * @param objectSources The sources of the objects.
     * @param objectNames   The names of the objects.
     */
    @VisibleForTesting
    @CalledByNative
    public void setRegisteredObjectIds(int[] objectSources, String[] objectNames) {
        InvalidationPreferences invalidationPreferences = new InvalidationPreferences();
        Account account = invalidationPreferences.getSavedSyncedAccount();
        Intent registerIntent = InvalidationIntentProtocol.createRegisterIntent(
                account, objectSources, objectNames);
        registerIntent.setClass(ContextUtils.getApplicationContext(),
                InvalidationClientService.getRegisteredClass());
        startServiceIfPossible(registerIntent);
    }

    private void startServiceIfPossible(Intent intent) {
        // The use of background services is restricted when the application is not in foreground
        // for O. See crbug.com/680812.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            try {
                ContextUtils.getApplicationContext().startService(intent);
            } catch (IllegalStateException exception) {
                Log.e(TAG, "Failed to start service from exception: ", exception);
            }
        } else {
            ContextUtils.getApplicationContext().startService(intent);
        }
    }

    /**
     * Fetches the Invalidator client name.
     *
     * Note that there is a naming discrepancy here.  In C++, we refer to the invalidation client
     * identifier that is unique for every invalidation client instance in an account as the client
     * ID.  In Java, we call it the client name.
     */
    @CalledByNative
    private byte[] getInvalidatorClientId() {
        return InvalidationClientNameProvider.get().getInvalidatorClientName();
    }

    @NativeMethods
    interface Natives {
        void invalidate(long nativeInvalidationServiceAndroid, InvalidationService caller,
                int objectSource, String objectId, long version, String payload);
    }
}
