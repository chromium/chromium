// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.LifetimeAssert;
import org.chromium.base.ThreadUtils;
import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.ui.base.WindowAndroid;

/**
 * Maintains a list of AutocompleteControllers associated with Profiles used by this Chrome
 * window. The controllers are not shared across windows, allowing windows to operate independently.
 */
public class AutocompleteControllerProvider implements UnownedUserData {
    private static final @NonNull UnownedUserDataKey<AutocompleteControllerProvider> KEY =
            new UnownedUserDataKey<>(AutocompleteControllerProvider.class);
    private static @Nullable AutocompleteController sControllerForTesting;
    private final @NonNull LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private final @NonNull ProfileKeyedMap<AutocompleteController> mControllers =
            ProfileKeyedMap.createMapOfDestroyables();

    /**
     * Autocloseable wrapper around the AutocompleteController.
     * Used when the code requesting AutocompleteController runs in headless mode.
     */
    public static class CloseableAutocompleteController implements AutoCloseable {
        private final @NonNull AutocompleteController mController;

        CloseableAutocompleteController(@NonNull Profile profile) {
            mController = new AutocompleteController(profile);
        }

        public @NonNull AutocompleteController get() {
            return mController;
        }

        @Override
        public void close() {
            mController.destroy();
        }
    }

    /**
     * Returns instance of the AutocompleteControllerProvider associated with the supplied
     * WindowAndroid. Must be called from the UI thread.
     *
     * @param windowAndroid The window to return the AutocompleteControllerProvider for.
     * @return Associated AutocompleteControllerProvider.
     */
    public static AutocompleteControllerProvider from(@NonNull WindowAndroid windowAndroid) {
        ThreadUtils.assertOnUiThread();
        assert windowAndroid != null;

        var userDataHost = windowAndroid.getUnownedUserDataHost();
        var factory = KEY.retrieveDataFromHost(userDataHost);
        if (factory == null) {
            factory = new AutocompleteControllerProvider();
            KEY.attachToHost(userDataHost, factory);
        }
        return factory;
    }

    @Override
    public void onDetachedFromHost(@NonNull UnownedUserDataHost host) {
        ThreadUtils.assertOnUiThread();
        mControllers.destroy();
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    /**
     * Retrieve the AutocompleteController appropriate for the supplied Profile.
     *
     * Must be called on the UI thread.
     *
     * @param profile The Profile to retrieve AutocompleteController for.
     * @return AutocompleteController associated with the supplied Profile.
     */
    public @NonNull AutocompleteController get(@NonNull Profile profile) {
        ThreadUtils.assertOnUiThread();

        if (sControllerForTesting != null) return sControllerForTesting;

        return mControllers.getForProfile(profile, () -> new AutocompleteController(profile));
    }

    /**
     * Retrieve the AutocompleteController instance that may be used in headless context.
     *
     * Note: This call is expensive. Use only when Chrome is running in Headless mode and it is
     * impossible to pass an instance WindowAndroid appropriate to the current context.
     */
    public static @NonNull CloseableAutocompleteController createCloseableController(
            @NonNull Profile profile) {
        ThreadUtils.assertOnUiThread();
        return new CloseableAutocompleteController(profile);
    }

    /**
     * Applies the user-supplied AutocompleteController instance as the instance to be
     * returned upon subsequent calls to AutocompleteControllerProvider#get(Profile).
     *
     * This method should only be used when it is needed to plumb the AutocompleteControllerProvider
     * deep, and it is impossible to pass a Mock or a Test instance directly to the tested class.
     * The caller must reset this value when it is no longer needed.
     *
     * @param provider Testing version of the AutocompleteControllerProvider, or null to reset the
     *         overridde.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static void setControllerForTesting(@Nullable AutocompleteController controller) {
        sControllerForTesting = controller;
    }
}
