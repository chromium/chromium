// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * An interface implemented by the embedder that allows the Accessibility Settings UI to access
 * embedder-specific logic.
 */
public interface AccessibilitySettingsDelegate {
    /** An interface to control a single boolean preference. */
    interface BooleanPreferenceDelegate {
        /** @return whether the preference is enabled. */
        boolean isEnabled();

        /** Called when the preference value is changed. */
        void setEnabled(boolean value);
    }

    /** An interface to control a single integer preference. */
    interface IntegerPreferenceDelegate {
        /** @return int - Current value of the preference of this instance. */
        int getValue();

        /**
         * Sets a new value for the preference of this instance.
         * @param value
         */
        void setValue(int value);
    }

    /** @return The BrowserContextHandle that should be used to read and update settings. */
    BrowserContextHandle getBrowserContextHandle();

    /**
     * @return the BooleanPreferenceDelegate instance that should be used when rendering the reader
     * for accessibility preference. Return null to omit the preference.
     */
    BooleanPreferenceDelegate getReaderForAccessibilityDelegate();

    /**
     * @return the InterPreferenceDelegate instance that should be used for reading and setting the
     * text size contrast value for accessibility settings. Return null to omit the preference.
     */
    IntegerPreferenceDelegate getTextSizeContrastAccessibilityDelegate();

    /**
     * Allows the embedder to add more preferences to the preference screen.
     *
     * @param fragment the fragment to add the preferences to.
     */
    void addExtraPreferences(@NonNull PreferenceFragmentCompat fragment);

    /** Returns whether or not the 'Zoom' feature specific UI should be shown in Settings. */
    boolean showPageZoomSettingsUI();

    /**
     * Launches a site settings category that displays zoom levels for each website.
     *
     * @param context the context from which to launch the activity from.
     */
    void launchSiteSettingsZoomActivity(Context context);
}
