// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.search_engines;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.Nullable;

import org.chromium.base.Promise;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.time.Instant;

/** Placeholder delegate class to get device country. Implemented in the internal code. */
public abstract class SearchEngineCountryDelegate {
    @MainThread
    public SearchEngineCountryDelegate() {}

    @MainThread
    public SearchEngineCountryDelegate(Context context) {}

    /**
     * Returns a {@link Promise} that will be fulfilled with the device country code. The promise
     * may be rejected if unable to fetch device country code. Clients should implement proper
     * callbacks to handle rejection. The promise is guaranteed to contain a non-null string.
     *
     * <p>If {@link SearchEnginesFeatures#CLAY_BLOCKING} is enabled, no rejection will be
     * propagated, the promise will be kept pending instead. Implement some timeout if that's
     * needed.
     */
    @MainThread
    public Promise<String> getDeviceCountry() {
        return Promise.rejected();
    }

    /** Proxy for {@link SearchEngineChoiceService#isDeviceChoiceDialogEligible()}. */
    @MainThread
    public boolean isDeviceChoiceDialogEligible() {
        return false;
    }

    /** Proxy for {@link SearchEngineChoiceService#getIsDeviceChoiceRequiredSupplier()}. */
    @MainThread
    public ObservableSupplier<Boolean> getIsDeviceChoiceRequiredSupplier() {
        return new ObservableSupplierImpl<>(false);
    }

    /** Proxy for {@link SearchEngineChoiceService#refreshDeviceChoiceRequiredNow}. */
    @MainThread
    public void refreshDeviceChoiceRequiredNow(int reason) {}

    /** Proxy for {@link SearchEngineChoiceService#launchDeviceChoiceScreens()}. */
    @MainThread
    public void launchDeviceChoiceScreens() {}

    /**
     * Returns the moment when the device recorded that the default browser has been selected by
     * the user in the OS-level choice screens.
     */
    public @Nullable Instant getDeviceBrowserSelectedTimestamp() {
        return null;
    }

    @IntDef({
        DeviceChoiceEventType.BLOCK_SHOWN,
        DeviceChoiceEventType.BLOCK_CLEARED,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DeviceChoiceEventType {
        /** See {@link SearchEngineChoiceService#notifyDeviceChoiceBlockShown()}. */
        int BLOCK_SHOWN = 1;

        /** See {@link SearchEngineChoiceService#notifyDeviceChoiceBlockCleared()}. */
        int BLOCK_CLEARED = 2;
    }

    /**
     * Proxy for device choice event notifications from {@link SearchEngineChoiceService}. See
     * {@link DeviceChoiceEventType} values for more details.
     */
    @MainThread
    public void notifyDeviceChoiceEvent(@DeviceChoiceEventType int eventType) {}
}
