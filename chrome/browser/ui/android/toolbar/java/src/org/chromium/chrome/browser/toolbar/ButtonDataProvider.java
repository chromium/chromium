// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;

/**
 * Interface for components that wish to display a button in the "optional" button slot in the
 * browsing mode toolbar.
 */
public interface ButtonDataProvider {
    /**
     * Observer class for changes in button state. Implementers should notify
     * their observers when their feature-specific display state changes,
     * e.g. the user signing out making the IdentityDisc non-displayable,
     * or drawable for the button changing.
     */
    interface ButtonDataObserver {
        /**
         * @param canShowHint Whether the provider thinks its button can be shown.
         * Embedders should always respect the hint's value when it is false,
         * but should only treat it as a hint when it is true; they will still
         * need to call get() to receive the complete picture of the button's
         * state.
         */
        void buttonDataChanged(boolean canShowHint);
    }

    /**
     * Add an observer that should be notified every time a provider's button data changes.
     */
    void addObserver(ButtonDataObserver obs);

    /**
     * Remove the given observer from the list of observers to notify.
     */
    void removeObserver(ButtonDataObserver obs);

    /**
     * Get the current ButtonData, including any tab-specific adjustments.
     */
    ButtonData get(@Nullable Tab tab);

    /**
     * Destroy the provider. This should be made safe to call multiple times, in case a provider is
     * held and destroyed as a separate, concrete instance in addition to a ButtonDataProvider.
     */
    void destroy();
}
