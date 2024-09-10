// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import androidx.annotation.MainThread;

import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;

/** Fake delegate that can be triggered in the app as a debug flag option, or used in tests. */
public class FakeSearchEngineCountryDelegate extends SearchEngineCountryDelegate {

    private static final String TAG = "FakeChoiceDelegate";

    private final Boolean mEnableLogging;
    private final ObservableSupplierImpl<Boolean> mIsChoiceRequired =
            new ObservableSupplierImpl<>(true);

    @MainThread
    public FakeSearchEngineCountryDelegate(boolean enableLogging) {
        super(/* context= */ null);
        ThreadUtils.assertOnUiThread();

        mEnableLogging = enableLogging;
        if (mEnableLogging) {
            if (SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)) {
                Log.i(TAG, "Initialising with ClayBlocking enabled");
                mIsChoiceRequired.addObserver(
                        value -> Log.i(TAG, "supplier value changed to %s", value));
            } else {
                Log.i(
                        TAG,
                        "Initialising with ClayBlocking disabled: Going silent and deferring to"
                                + " base implementation.");
            }
        }
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
                    mIsChoiceRequired.get());
        }
        return mIsChoiceRequired;
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
        mIsChoiceRequired.set(false);
    }

    @Override
    @MainThread
    public void log(@DeviceChoiceEventType int eventType) {
        ThreadUtils.assertOnUiThread();
        if (!SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)) {
            super.log(eventType);
        }

        if (mEnableLogging) {
            Log.i(TAG, "log(%d)", eventType);
        }
    }
}
