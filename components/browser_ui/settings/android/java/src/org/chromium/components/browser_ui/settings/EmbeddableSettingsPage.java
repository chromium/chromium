// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;

/**
 * The base interface for embeddable settings page fragments.
 *
 * <p>Fragments implementing this interface are called <i>embeddable</i>; otherwise they are
 * considered <i>standalone</i>.
 *
 * <p>Embeddable fragments can be a part of the multi-column UI (if it is enabled). They must not
 * modify UI outside of the fragment. For example, it should implement {@link getPageTitle} to
 * provide the page title, instead of modifying the activity title directly.
 *
 * <p>Standalone fragments are shown as a whole and has better control of the activity.
 */
@NullMarked
public interface EmbeddableSettingsPage extends SettingsFragment {
    /**
     * Returns the title of the current setting page.
     *
     * <p>The setting page title may not be shown as the activity's title. Implement this method to
     * specify the setting page title, instead of directly modifying the activity title.
     *
     * <p>The activity will observe changes to this value and update the UI as necessary.
     */
    ObservableSupplier<String> getPageTitle();

    /**
     * Returns the "key" tag of the main_preference, or null.
     *
     * <p>If non null, the corresponding item in the main_preference is highlighted when this
     * fragment is opened and at the bottom of the back stack.
     */
    // TODO(crbug.com/454312815): The value returned from here should be taken from xml file
    // to keep the consistency with the main_preferences.xml.
    default @Nullable String getMainMenuKey() {
        return null;
    }
}
