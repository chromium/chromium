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

/** Interface for navigating Settings. */
public interface SettingsNavigation {
    @IntDef({
        SettingsFragment.MAIN,
        SettingsFragment.CLEAR_BROWSING_DATA,
        SettingsFragment.CLEAR_BROWSING_DATA_ADVANCED_PAGE,
        SettingsFragment.PAYMENT_METHODS,
        SettingsFragment.SAFETY_CHECK,
        SettingsFragment.SITE,
        SettingsFragment.ACCESSIBILITY,
        SettingsFragment.PASSWORDS,
        SettingsFragment.GOOGLE_SERVICES,
        SettingsFragment.MANAGE_SYNC,
        SettingsFragment.FINANCIAL_ACCOUNTS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface SettingsFragment {
        /// Main settings page.
        int MAIN = 0;
        /// Browsing Data management.
        int CLEAR_BROWSING_DATA = 1;
        /// Advanced page of browsing data management.
        int CLEAR_BROWSING_DATA_ADVANCED_PAGE = 2;
        /// Payment methods and autofill settings.
        int PAYMENT_METHODS = 3;
        /// Safety check, automatically running the action.
        int SAFETY_CHECK = 4;
        /// Site settings and permissions.
        int SITE = 5;
        /// Accessibility settings.
        int ACCESSIBILITY = 6;
        /// Password settings.
        int PASSWORDS = 7;
        /// Google services.
        int GOOGLE_SERVICES = 8;
        /// Manage sync.
        int MANAGE_SYNC = 9;
        /// Financial accounts.
        int FINANCIAL_ACCOUNTS = 10;
    }

    /**
     * Starts settings with the default (top-level) fragment.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     */
    void startSettings(Context context);

    /**
     * Starts a specific settings fragment. This can be used by code that does not supply its own
     * settings page, but instead needs to redirect the user to an appropriate page that is out of
     * reach.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param settingsFragment The {@link SettingsFragment} to run.
     */
    void startSettings(Context context, @SettingsFragment int settingsFragment);

    /**
     * Starts settings with the specified fragment.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The fragment to show, or null to show the default fragment.
     */
    void startSettings(Context context, @Nullable Class<? extends Fragment> fragment);

    /**
     * Starts settings with the specified fragment and arguments.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The fragment to show, or null to show the default fragment.
     * @param fragmentArgs A bundle of additional fragment arguments.
     */
    void startSettings(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs);

    /**
     * Creates an intent for starting settings with the specified fragment.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The class of the fragment to show, or null to show the default fragment.
     */
    Intent createSettingsIntent(Context context, @Nullable Class<? extends Fragment> fragment);

    /**
     * Creates an intent for starting settings with the specified fragment and arguments.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The class of the fragment to show, or null to show the default fragment.
     * @param fragmentArgs A bundle of additional fragment arguments.
     */
    Intent createSettingsIntent(
            Context context,
            @Nullable Class<? extends Fragment> fragment,
            @Nullable Bundle fragmentArgs);

    /**
     * Creates an intent for starting settings with the specified fragment and arguments.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The fragment to show.
     * @param fragmentArgs A bundle of additional fragment arguments.
     */
    Intent createSettingsIntent(
            Context context, @SettingsFragment int fragment, @Nullable Bundle fragmentArgs);

    /**
     * Finishes the current settings.
     *
     * <p>Call this method when the user is done with the current fragment and should go back to the
     * previous fragment (e.g. selected a language from the language list). If the given fragment is
     * not the current one, this method does nothing.
     *
     * @param fragment The expected current fragment.
     */
    void finishCurrentSettings(Fragment fragment);
}
