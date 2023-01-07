// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

/**
 * Interface for launching Settings.
 */
public interface SettingsLauncher {
    /**
     * Launches a Settings Activity with the default (top-level) fragment.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     */
    void launchSettingsActivity(Context context);

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
