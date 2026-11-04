// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.containment;

import androidx.preference.Preference;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Assigns preferences to visual sections without {@link androidx.preference.PreferenceCategory}
 * headers. Containment starts a new card wherever two neighboring visible preferences report
 * different section ids.
 */
@NullMarked
public interface SectionIdProvider {
    /**
     * Returns the id of the section {@code preference} belongs to, or null if it is not grouped.
     * Neighbors with equal ids (including both null) share a card.
     */
    @Nullable String getSectionId(Preference preference);
}
