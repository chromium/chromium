// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;

import androidx.annotation.NonNull;

/** An interface that provides and notifies about night mode state. */
public interface NightModeStateProvider {
    /** Observes night mode state changes. */
    interface Observer {
        /** Notifies on night mode state changed. */
        void onNightModeStateChanged();
    }

    /** @return Whether or not night mode is enabled. */
    boolean isInNightMode();

    /** @param observer The {@link Observer} to be registered to this provider. */
    void addObserver(@NonNull Observer observer);

    /** @param observer The {@link Observer} to be unregistered to this provider. */
    void removeObserver(@NonNull Observer observer);

    /**
     * @return Whether or not {@link Configuration#uiMode} should be overridden for night mode by
     *         {@link Activity#applyOverrideConfiguration(Configuration)}. This is applicable when
     *         an Activity configures whether night mode is enabled (e.g. through a user setting)
     *         rather than relying on the Application context UI night mode.
     *         Note that if night mode state is initialized after
     *         {@link Activity#attachBaseContext(Context)}, this should return false.
     */
    default boolean shouldOverrideConfiguration() {
        return true;
    }
}
