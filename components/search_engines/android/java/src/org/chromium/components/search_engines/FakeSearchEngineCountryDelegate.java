// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.time.Instant;

/** Fake delegate that can be triggered in the app as a debug flag option, or used in tests. */
@NullMarked
public class FakeSearchEngineCountryDelegate extends SearchEngineCountryDelegate {
    private static final String TAG = "SearchEngineDelefake";

    private static final int CHOICE_REQUIRED_DELAY_MS = 3000;

    private final boolean mEnableLogging;
    private @MonotonicNonNull SettableNullableObservableSupplier<Boolean> mIsChoiceRequired;

    /**
     * Supplier used as the main way to mock the search engine choice device backend.
     *
     * <p>Makes {@link #getIsDeviceChoiceRequiredSupplier()}'s supplier return {@code null} on
     * start, emits {@code true} after 3 seconds, indicating that the blocking dialog should be
     * shown, and updates it to {@code false} when {@link #launchDeviceChoiceScreens()} is called.
     */
    @MainThread
    public FakeSearchEngineCountryDelegate(boolean enableLogging) {
        ThreadUtils.assertOnUiThread();

        mEnableLogging = enableLogging;
    }

    @VisibleForTesting
    public void setIsDeviceChoiceRequired(Boolean isRequired) {
        getSupplier().set(isRequired);
    }

    @Override
    @MainThread
    public Promise<String> getDeviceCountry() {
        ThreadUtils.assertOnUiThread();

        String countryCode = "IE"; // CLDR country code for Ireland.
        if (mEnableLogging) {
            Log.i(TAG, "getDeviceCountry() -> promise fulfilled with %s", countryCode);
        }
        return Promise.fulfilled(countryCode);
    }

    @Override
    public @Nullable Instant getDeviceBrowserSelectedTimestamp() {
        if (mEnableLogging) {
            Log.i(TAG, "getDeviceBrowserSelectedTimestamp()");
        }
        return null;
    }

    @Override
    @MainThread
    public boolean isDeviceChoiceDialogEligible() {
        ThreadUtils.assertOnUiThread();

        if (mEnableLogging) {
            Log.i(TAG, "isDeviceChoiceDialogEligible() -> true");
        }
        return true;
    }

    @Override
    @MainThread
    public NullableObservableSupplier<Boolean> getIsDeviceChoiceRequiredSupplier() {
        ThreadUtils.assertOnUiThread();

        if (mEnableLogging) {
            Log.i(
                    TAG,
                    "getIsDeviceChoiceRequiredSupplier() -> current value: %s",
                    getSupplier().get());
        }
        return getSupplier();
    }

    @Override
    public void refreshDeviceChoiceRequiredNow(int reason) {
        if (mEnableLogging) {
            Log.i(TAG, "refreshDeviceChoiceRequiredNow()");
        }
    }

    @Override
    @MainThread
    public void launchDeviceChoiceScreens() {
        ThreadUtils.assertOnUiThread();

        if (mEnableLogging) {
            Log.i(TAG, "launchDeviceChoiceScreens() -> updating supplier");
        }
        getSupplier().set(false);
    }

    @Override
    @MainThread
    public void notifyDeviceChoiceEvent(@DeviceChoiceEventType int eventType) {
        ThreadUtils.assertOnUiThread();

        if (mEnableLogging) {
            Log.i(TAG, "notifyDeviceChoiceEvent(%d)", eventType);
        }
    }

    private SettableNullableObservableSupplier<Boolean> getSupplier() {
        // The supplier is lazily initialized, to more closely match the behaviour of the real
        // implementation, which does not trigger connections and queries unless the supplier is
        // needed.
        if (mIsChoiceRequired == null) {
            // Fake the backend taking some time to respond.
            mIsChoiceRequired = ObservableSuppliers.createNullable();
            ThreadUtils.postOnUiThreadDelayed(
                    () -> {
                        if (mEnableLogging) {
                            Log.i(TAG, "triggering the delayed supplier response.");
                        }
                        mIsChoiceRequired.set(true);
                    },
                    CHOICE_REQUIRED_DELAY_MS);

            if (mEnableLogging) {
                mIsChoiceRequired.addSyncObserverAndPostIfNonNull(
                        value -> Log.i(TAG, "supplier value changed to %s", value));
            }
        }

        return mIsChoiceRequired;
    }
}
