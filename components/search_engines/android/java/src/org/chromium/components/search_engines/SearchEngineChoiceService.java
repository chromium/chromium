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
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.base.supplier.TransitiveObservableSupplier;
import org.chromium.components.search_engines.SearchEngineChoiceServiceDelegate.DeviceChoiceEventType;

/**
 * Singleton responsible for communicating with device APIs to expose device-level properties that
 * are relevant for search engine choice screens and other similar UIs. It is the Java counterpart
 * of the native `SearchEngineChoiceService`, propagating some of the properties (notably the device
 * country string from {@link SearchEngineChoiceServiceDelegate}) to C++ instances of
 * `SearchEngineChoiceService`.
 *
 * <p>The object is a singleton rather than being profile-scoped as device properties apply to all
 * profiles, it also allows an instance to be created before the native is initialized.
 */
public class SearchEngineChoiceService {
    private static final String TAG = "DeviceChoiceDialog";
    private static SearchEngineChoiceService sInstance;

    /**
     * Gets reset to {@code null} after the device country is obtained.
     *
     * <p>TODO(b/355054098): Rely on disconnections inside the delegate instead of giving it up to
     * garbage collection. This will allow reconnecting if we need the delegate for other purposes.
     */
    private @Nullable SearchEngineChoiceServiceDelegate mDelegate;

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
            var context = ContextUtils.getApplicationContext();
            var delegate =
                    SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)
                                    && SearchEnginesFeatureUtils.clayBlockingUseFakeBackend()
                            ? new FakeSearchEngineChoiceServiceDelegate(/* enableLogging= */ true)
                            : new SearchEngineChoiceServiceDelegateImpl(context);
            sInstance = new SearchEngineChoiceService(delegate);
        }
        return sInstance;
    }

    /** Overrides the instance of the singleton for tests. */
    @MainThread
    @VisibleForTesting
    public static void setInstanceForTests(SearchEngineChoiceService instance) {
        ThreadUtils.checkUiThread();
        sInstance = instance;
        if (instance != null) {
            ResettersForTesting.register(() -> setInstanceForTests(null)); // IN-TEST
        }
    }

    @VisibleForTesting
    @MainThread
    public SearchEngineChoiceService(@NonNull SearchEngineChoiceServiceDelegate delegate) {
        ThreadUtils.checkUiThread();
        mDelegate = delegate;

        mDeviceCountryPromise = mDelegate.getDeviceCountry();

        mDeviceCountryPromise.then(
                countryCode -> {
                    assert countryCode != null : "Contract violation, country code should be null";
                },
                unusedException -> {});

        if (!SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)) {
            // We request the country code once per run, so it is safe to free up
            // the delegate now.
            mDeviceCountryPromise.andFinally(() -> mDelegate = null);
        }
    }

    /**
     * Returns a promise that will resolve to a CLDR country code, see
     * https://www.unicode.org/cldr/charts/45/supplemental/territory_containment_un_m_49.html.
     * Fulfilled promises are guaranteed to return a non-nullable string, but rejected ones also
     * need to be handled, indicating some error in obtaining the device country.
     *
     * <p>If {@link SearchEnginesFeatures#CLAY_BLOCKING} is enabled, no rejection will be
     * propagated, the promise will be kept pending instead. Implement some timeout if that's
     * needed.
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
     * <p>This call might be relying on cached data, and the result of {@link
     * #shouldShowDeviceChoiceDialog} or {@link #getIsDeviceChoiceRequiredSupplier} should be
     * checked afterwards to ensure that the dialog is actually required.
     */
    @MainThread
    public boolean isDeviceChoiceDialogEligible() {
        ThreadUtils.checkUiThread();
        if (!SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)) return false;
        assert mDelegate != null;

        return mDelegate.isDeviceChoiceDialogEligible();
    }

    /**
     * Returns a {@link Promise} that will be fulfilled with a determination of whether the user is
     * required to complete their choices of system default apps before continuing to use this app.
     *
     * @deprecated Prefer using {@link #getIsDeviceChoiceRequiredSupplier()} instead.
     */
    @Deprecated
    @MainThread
    public Promise<Boolean> shouldShowDeviceChoiceDialog() {
        ThreadUtils.checkUiThread();
        if (!SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)) {
            return Promise.rejected();
        }

        var promise = new Promise<Boolean>();
        new OneShotCallback<>(getIsDeviceChoiceRequiredSupplier(), promise::fulfill);
        return promise;
    }

    /**
     * Supplier allowing to subscribe to changes in whether Chrome should require the user to
     * complete the device choices.
     *
     * <p>Possible return values:
     *
     * <ul>
     *   <li>null/no value: The service is not currently connected.
     *   <li>true: The dialog should be shown and block.
     *   <li>false: Blocking is not needed.
     * </ul>
     */
    @MainThread
    public ObservableSupplier<Boolean> getIsDeviceChoiceRequiredSupplier() {
        ThreadUtils.checkUiThread();
        var alwaysFalseSupplier = new ObservableSupplierImpl<>(false);

        if (!SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)) {
            return alwaysFalseSupplier;
        }

        assert mDelegate != null;

        var supplier = mDelegate.getIsDeviceChoiceRequiredSupplier();
        if (SearchEnginesFeatureUtils.clayBlockingIsDarkLaunch()) {
            // We want to call into the backend to be able to verify it's working, but we intercept
            // its returned values to prevent it from affecting the user experience.
            return new TransitiveObservableSupplier<>(supplier, ignored -> alwaysFalseSupplier);
        }

        return supplier;
    }

    /**
     * Requests the device to launch its flow allowing the user to complete their choices of system
     * default apps.
     */
    @MainThread
    public void launchDeviceChoiceScreens() {
        ThreadUtils.checkUiThread();
        if (!SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)) {
            return;
        }

        assert !SearchEnginesFeatureUtils.clayBlockingIsDarkLaunch();
        assert mDelegate != null;
        Log.i(TAG, "launchChoiceScreens()");
        mDelegate.launchDeviceChoiceScreens();
    }

    /** Notifies the service that the UI preventing the user from using the app has been shown. */
    @MainThread
    public void notifyDeviceChoiceBlockShown() {
        notifyDeviceChoiceEvent(DeviceChoiceEventType.BLOCK_SHOWN);
    }

    /** Notifies the service that the UI preventing the user from using the app has been removed. */
    @MainThread
    public void notifyDeviceChoiceBlockCleared() {
        notifyDeviceChoiceEvent(DeviceChoiceEventType.BLOCK_CLEARED);
    }

    /**
     * To be called when some key events (see {@link DeviceChoiceEventType}) happen in the app UI.
     *
     * <p>Private because {@link DeviceChoiceEventType} has to be part of the delegate API
     * definition, not of the service API definition. (build targets setup limitation).
     */
    @MainThread
    private void notifyDeviceChoiceEvent(@DeviceChoiceEventType int eventType) {
        ThreadUtils.checkUiThread();
        if (!SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)) {
            return;
        }

        assert mDelegate != null;
        Log.i(TAG, "notifyDeviceChoiceEvent(%d)", eventType);
        mDelegate.notifyDeviceChoiceEvent(eventType);
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
