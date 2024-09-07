// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import org.chromium.base.supplier.ObservableSupplier;

/** The base interface that all setting page fragments should implement. */
public interface SettingsPage {
    /**
     * Returns the title of the current setting page.
     *
     * <p>The setting page title may not be shown as the activity's title. Implement this method to
     * specify the setting page title, instead of directly modifying the activity title.
     *
     * <p>The activity will observe changes to this value and update the UI as necessary.
     */
    ObservableSupplier<String> getPageTitle();
}
