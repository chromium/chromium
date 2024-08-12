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

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

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

    private final List<Long> mPtrToNativeCallbacks = new ArrayList<>();
    // To understand whether we already got a reply from `SearchEngineCountryDelegate`, we need to
    // differentiate between "null result" and "no result yet", thus the optional.
    private @Nullable Optional<String> mPlayCountryRequestResult;

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

        mDelegate
                .getDeviceCountry()
                .then(
                        this::processResponseFromPlayApi,
                        unusedException -> processResponseFromPlayApi(null));
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

    @CalledByNative
    private static void requestCountryFromPlayApi(long ptrToNativeCallback) {
        ThreadUtils.checkUiThread();
        getInstance().requestCountryFromPlayApiInternal(ptrToNativeCallback);
    }

    private void requestCountryFromPlayApiInternal(long ptrToNativeCallback) {
        if (mPlayCountryRequestResult != null) {
            // The result is ready - call native so it can save the result in prefs.
            SearchEngineChoiceServiceJni.get()
                    .processCountryFromPlayApi(
                            ptrToNativeCallback, mPlayCountryRequestResult.orElse(null));
            return;
        }
        // When `SearchEngineCountryDelegate` replies with the result - the result will be reported
        // to native using the saved callback.
        mPtrToNativeCallbacks.add(ptrToNativeCallback);
    }

    /**
     * Saves the result of the device country request and propagates to native callbacks waiting for
     * this result.
     *
     * @param deviceCountry the country code string or null if there was an error.
     */
    private void processResponseFromPlayApi(@Nullable String deviceCountry) {
        ThreadUtils.checkUiThread();
        assert mPlayCountryRequestResult == null;
        mPlayCountryRequestResult = Optional.ofNullable(deviceCountry);
        for (long ptrToNativeCallback : mPtrToNativeCallbacks) {
            // `mPtrToNativeCallbacks` can be non-empty only after the native is loaded, so it is
            // safe to call JNI here.
            SearchEngineChoiceServiceJni.get()
                    .processCountryFromPlayApi(ptrToNativeCallback, deviceCountry);
        }
        mPtrToNativeCallbacks.clear();
        // We request the country code once per run, so it is safe to free up the delegate now.
        mDelegate = null;
    }

    @NativeMethods
    public interface Natives {
        void processCountryFromPlayApi(long ptrToNativeCallback, @Nullable String deviceCountry);
    }
}
