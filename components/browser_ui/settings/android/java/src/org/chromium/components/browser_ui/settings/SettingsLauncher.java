// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Interface for launching Settings.
 */
public interface SettingsLauncher {
    @IntDef({SettingsFragment.MAIN, SettingsFragment.CLEAR_BROWSING_DATA,
            SettingsFragment.PAYMENT_METHODS, SettingsFragment.SAFETY_CHECK, SettingsFragment.SITE,
            SettingsFragment.ACCESSIBILITY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SettingsFragment {
        /// Main settings page.
        int MAIN = 0;
        /// Browsing Data management.
        int CLEAR_BROWSING_DATA = 1;
        /// Payment methods and autofill settings.
        int PAYMENT_METHODS = 2;
        /// Safety check, automatically running the action.
        int SAFETY_CHECK = 3;
        /// Site settings and permissions.
        int SITE = 4;
        /// Accessibility settings.
        int ACCESSIBILITY = 5;
    }

    /**
     * Launches a Settings Activity with the default (top-level) fragment.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     */
    void launchSettingsActivity(Context context);

    /**
     * Launches a specific Settings Activity fragment. This can be used by code that does not supply
     * its own settings page, but instead needs to redirect the user to an appropriate page that is
     * out of reach.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param settingsFragment The {@link SettingsFragment} to run.
     */
    void launchSettingsActivity(Context context, @SettingsFragment int settingsFragment);

    /**
     * Launches a Settings Activity with the specified fragment.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The fragment to show, or null to show the default fragment.
     */
    void launchSettingsActivity(Context context, @Nullable Class<? extends Fragment> fragment);

    /**
     * Launches a Settings Activity with the specified fragment and arguments.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The fragment to show, or null to show the default fragment.
     * @param fragmentArgs A bundle of additional fragment arguments.
     */
    void launchSettingsActivity(Context context, @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs);

    /**
     * Creates an intent for launching a Settings Activity with the specified fragment.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragmentName The name of the fragment to show, or null to show the default fragment.
     */
    Intent createSettingsActivityIntent(Context context, @Nullable String fragmentName);

    /**
     * Creates an intent for launching a Settings Activity with the specified fragment and
     * arguments.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragmentName The name of the fragment to show, or null to show the default fragment.
     * @param fragmentArgs A bundle of additional fragment arguments.
     */
    Intent createSettingsActivityIntent(
            Context context, @Nullable String fragmentName, @Nullable Bundle fragmentArgs);
}
