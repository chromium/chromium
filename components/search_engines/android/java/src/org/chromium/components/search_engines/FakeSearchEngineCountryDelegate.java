// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;

/** Fake delegate that can be triggered in the app as a debug flag option, or used in tests. */
public class FakeSearchEngineCountryDelegate extends SearchEngineCountryDelegate {
    private static final String TAG = "SearchEngineDelefake";

    private final boolean mEnableLogging;
    private @Nullable ObservableSupplierImpl<Boolean> mIsChoiceRequired;

    /**
     * Supplier used as the main way to mock the search engine choice device backend.
     *
     * <p>Makes {@link #getIsDeviceChoiceRequiredSupplier()}'s supplier return {@code true} on
     * start, indicating that the blocking dialog should be shown, and updates it to {@code false}
     * when {@link #launchDeviceChoiceScreens()} is called. If a timeout is configured (see {@link
     * SearchEnginesFeatureUtils#clayBlockingDialogTimeoutMillis()}, the initial value will be
     * emitted at half of the timeout duration instead, to exercise the delayed response path.
     */
    @MainThread
    public FakeSearchEngineCountryDelegate(boolean enableLogging) {
        ThreadUtils.assertOnUiThread();

        mEnableLogging = enableLogging;
        if (mEnableLogging) {
            if (SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)) {
                Log.i(TAG, "Initialising with ClayBlocking enabled");
            } else {
                Log.i(
                        TAG,
                        "Initialising with ClayBlocking disabled: Going silent and deferring to"
                                + " base implementation.");
            }
        }
    }

    @VisibleForTesting
    public void setIsDeviceChoiceRequired(Boolean isRequired) {
        getSupplier().set(isRequired);
    }

    @Override
    @MainThread
    public Promise<String> getDeviceCountry() {
        ThreadUtils.assertOnUiThread();
        if (!SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)) {
            return super.getDeviceCountry();
        }

        String countryCode = "IE"; // CLDR country code for Ireland.
        if (mEnableLogging) {
            Log.i(TAG, "getDeviceCountry() -> promise fulfilled with %s", countryCode);
        }
        return Promise.fulfilled(countryCode);
    }

    @Override
    @MainThread
    public boolean isDeviceChoiceDialogEligible() {
        ThreadUtils.assertOnUiThread();
        if (!SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)) {
            return super.isDeviceChoiceDialogEligible();
        }

        if (mEnableLogging) {
            Log.i(TAG, "isDeviceChoiceDialogEligible() -> true");
        }
        return true;
    }

    @Override
    @MainThread
    public ObservableSupplier<Boolean> getIsDeviceChoiceRequiredSupplier() {
        ThreadUtils.assertOnUiThread();
        if (!SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)) {
            return super.getIsDeviceChoiceRequiredSupplier();
        }

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
        if (!SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)) {
            super.launchDeviceChoiceScreens();
        }

        if (mEnableLogging) {
            Log.i(TAG, "launchDeviceChoiceScreens() -> updating supplier");
        }
        getSupplier().set(false);
    }

    @Override
    @MainThread
    public void notifyDeviceChoiceEvent(@DeviceChoiceEventType int eventType) {
        ThreadUtils.assertOnUiThread();
        if (!SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)) {
            super.notifyDeviceChoiceEvent(eventType);
        }

        if (mEnableLogging) {
            Log.i(TAG, "notifyDeviceChoiceEvent(%d)", eventType);
        }
    }

    private ObservableSupplierImpl<Boolean> getSupplier() {
        // The supplier is lazily initialized, to more closely match the behaviour of the real
        // implementation, which does not trigger connections and queries unless the supplier is
        // needed.
        if (mIsChoiceRequired == null) {
            int dialogTimeoutMillis = SearchEnginesFeatureUtils.clayBlockingDialogTimeoutMillis();
            if (dialogTimeoutMillis > 0) {
                // A dialog timeout is configured, so make the fake delegate exercise it: Start with
                // no provided response, but emit the `true` value halfway to the deadline.
                mIsChoiceRequired = new ObservableSupplierImpl<>();
                ThreadUtils.postOnUiThreadDelayed(
                        () -> {
                            if (mEnableLogging) {
                                Log.i(TAG, "triggering the delayed supplier response.");
                            }
                            mIsChoiceRequired.set(true);
                        },
                        // Don't go beyond 3 seconds timeout, it doesn't help with testing and looks
                        // broken.
                        Math.min(dialogTimeoutMillis / 2, 3000));
            } else {
                mIsChoiceRequired = new ObservableSupplierImpl<>(true);
            }

            if (mEnableLogging) {
                mIsChoiceRequired.addObserver(
                        value -> Log.i(TAG, "supplier value changed to %s", value));
            }
        }

        return mIsChoiceRequired;
    }
}
