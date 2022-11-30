// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

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
        /**
         * @return whether the preference is enabled.
         */
        boolean isEnabled();

        /**
         * Called when the preference value is changed.
         */
        void setEnabled(boolean value);
    }

    /**
     * @return The BrowserContextHandle that should be used to read and update settings.
     */
    BrowserContextHandle getBrowserContextHandle();

    /**
     * @return the BooleanPreferenceDelegate instance that should be used when rendering the
     * accessibility tab switcher preference. Return null to omit the preference.
     */
    BooleanPreferenceDelegate getAccessibilityTabSwitcherDelegate();

    /**
     * @return the BooleanPreferenceDelegate instance that should be used when rendering the reader
     * for accessibility preference. Return null to omit the preference.
     */
    BooleanPreferenceDelegate getReaderForAccessibilityDelegate();

    /**
     * Allows the embedder to add more preferences to the preference screen.
     *
     * @param fragment the fragment to add the preferences to.
     */
    void addExtraPreferences(@NonNull PreferenceFragmentCompat fragment);

    /**
     * Returns whether or not the 'Zoom' feature specific UI should be shown in Settings.
     */
    boolean showPageZoomSettingsUI();
}
