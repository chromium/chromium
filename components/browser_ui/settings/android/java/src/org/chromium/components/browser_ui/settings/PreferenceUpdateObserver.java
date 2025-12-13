// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.build.annotations.NullMarked;

/**
 * An observer that is notified when the list of visible preferences in a settings fragment is
 * updated. This can happen when preferences are dynamically displayed or hidden.
 */
@NullMarked
public interface PreferenceUpdateObserver {
    /** Called when the list of preferences has been updated. */
    void onPreferencesUpdated(PreferenceFragmentCompat fragment);

    /** An interface for fragments to implement to receive the {@link PreferenceUpdateObserver}. */
    interface Provider {
        /** Sets the {@link PreferenceUpdateObserver}. */
        void setPreferenceUpdateObserver(PreferenceUpdateObserver observer);

        /** Removes the {@link PreferenceUpdateObserver}. */
        void removePreferenceUpdateObserver();
    }
}
