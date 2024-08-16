// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.search_engines;

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Promise;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;

/**
 * Java counterpart of the native `SearchEngineChoiceService`. This singleton is responsible for
 * getting the device country string from {@link SearchEngineCountryDelegate} and propagating it to
 * C++ instances of `SearchEngineChoiceService`. The object is a singleton rather than being
 * profile-scoped as the device country obtained from `SearchEngineCountryDelegate` is global (it
 * also allows `SearchEngineChoiceService` instance to be created before the native is initialized).
 */
public class SearchEngineChoiceService {
    private static SearchEngineChoiceService sInstance;

    /**
     * Gets reset to {@code null} after the device country is obtained.
     *
     * <p>TODO(b/355054098): Rely on disconnections inside the delegate instead of giving it up to
     * garbage collection. This will allow reconnecting if we need the delegate for other purposes.
     */
    private @Nullable SearchEngineCountryDelegate mDelegate;

    /**
     * Cached status associated with initiating a device country fetch when the object is
     * instantiated.
     *
     * <p>Possible states:
     *
     * <ul>
     *   <li>Pending: The fetch is not completed.
     *   <li>Fulfilled: The fetch succeeded, the value should be a non-null String. (note: it might
     *       still be an invalid or unknown country code!)
     *   <li>Rejected: An error occurred.
     * </ul>
     */
    private final Promise<String> mDeviceCountryPromise;

    /** Returns the instance of the singleton. Creates the instance if needed. */
    @MainThread
    public static SearchEngineChoiceService getInstance() {
        ThreadUtils.checkUiThread();
        if (sInstance == null) {
            sInstance =
                    new SearchEngineChoiceService(
                            new SearchEngineCountryDelegateImpl(
                                    ContextUtils.getApplicationContext()));
        }
        return sInstance;
    }

    /** Overrides the instance of the singleton for tests. */
    public static void setInstanceForTests(SearchEngineChoiceService instance) {
        ThreadUtils.checkUiThread();
        sInstance = instance;
        if (instance != null) {
            ResettersForTesting.register(() -> setInstanceForTests(null)); // IN-TEST
        }
    }

    @VisibleForTesting
    public SearchEngineChoiceService(@NonNull SearchEngineCountryDelegate delegate) {
        ThreadUtils.checkUiThread();
        mDelegate = delegate;

        mDeviceCountryPromise = mDelegate.getDeviceCountry();

        mDeviceCountryPromise
                .then(
                        (String countryCode) -> {
                            assert countryCode != null
                                    : "Contract violation, country code should be null";
                            return countryCode;
                        })
                .andFinally(
                        () -> {
                            // We request the country code once per run, so it is safe to free up
                            // the delegate now.
                            mDelegate = null;
                        });
    }

    /**
     * Returns a promise that will resolve to a CLDR country code, see
     * https://www.unicode.org/cldr/charts/45/supplemental/territory_containment_un_m_49.html.
     * Fulfilled promises are guaranteed to return a non-nullable string, but rejected ones also
     * need to be handled, indicating some error in obtaining the device country.
     *
     * <p>TODO(b/328040066): Ensure this is ACL'ed.
     */
    @MainThread
    public Promise<String> getDeviceCountry() {
        ThreadUtils.checkUiThread();
        return mDeviceCountryPromise;
    }

    /**
     * Returns whether the app should attempt to prompt the user to complete their choices of system
     * default apps.
     *
     * <p>This call might be relying on cached data, and {@link #shouldShowDeviceChoiceDialog}
     * should be called afterwards to ensure that the dialog is actually required.
     */
    @MainThread
    public boolean isDeviceChoiceDialogEligible() {
        if (mDelegate == null) return false;
        return mDelegate.isDeviceChoiceDialogEligible();
    }

    /**
     * Returns a {@link Promise} that will be fulfilled with whether the app should prompt the user
     * to complete their choices of default system apps.
     */
    @MainThread
    public Promise<Boolean> shouldShowDeviceChoiceDialog() {
        ThreadUtils.checkUiThread();
        if (mDelegate == null) return Promise.rejected();
        return mDelegate.shouldShowDeviceChoiceDialog();
    }

    private void requestCountryFromPlayApiInternal(long ptrToNativeCallback) {
        if (mDeviceCountryPromise.isPending()) {
            // When `SearchEngineCountryDelegate` replies with the result - the result will be
            // reported to native using the queued callback.
            mDeviceCountryPromise.then(
                    deviceCountry ->
                            SearchEngineChoiceServiceJni.get()
                                    .processCountryFromPlayApi(ptrToNativeCallback, deviceCountry),
                    ignoredException ->
                            SearchEngineChoiceServiceJni.get()
                                    .processCountryFromPlayApi(ptrToNativeCallback, null));
            return;
        }
        // The result is ready - call native so it can save the result in prefs.
        SearchEngineChoiceServiceJni.get()
                .processCountryFromPlayApi(
                        ptrToNativeCallback,
                        mDeviceCountryPromise.isFulfilled()
                                ? mDeviceCountryPromise.getResult()
                                : null);
    }

    @CalledByNative
    private static void requestCountryFromPlayApi(long ptrToNativeCallback) {
        ThreadUtils.checkUiThread();
        getInstance().requestCountryFromPlayApiInternal(ptrToNativeCallback);
    }

    @NativeMethods
    public interface Natives {
        void processCountryFromPlayApi(long ptrToNativeCallback, @Nullable String deviceCountry);
    }
}
