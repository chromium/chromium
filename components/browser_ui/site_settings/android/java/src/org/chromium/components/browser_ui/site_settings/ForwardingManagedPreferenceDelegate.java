// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import androidx.annotation.LayoutRes;
import androidx.preference.Preference;

import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;

/**
 * A ManagedPreferenceDelegate that forwards all method calls to a base ManagedPreferenceDelegate
 * instance.
 *
 * Methods in this class should be overridden to provide custom behavior. Non-overridden methods
 * will forward to the base implementation, which will typically be the embedder-provided
 * ManagedPreferenceDelegate instance.
 */
public class ForwardingManagedPreferenceDelegate implements ManagedPreferenceDelegate {
    private final ManagedPreferenceDelegate mBase;

    public ForwardingManagedPreferenceDelegate(ManagedPreferenceDelegate base) {
        this.mBase = base;
    }

    @Override
    public boolean isPreferenceControlledByPolicy(Preference preference) {
        return mBase.isPreferenceControlledByPolicy(preference);
    }

    @Override
    public boolean isPreferenceControlledByCustodian(Preference preference) {
        return mBase.isPreferenceControlledByCustodian(preference);
    }

    @Override
    public boolean doesProfileHaveMultipleCustodians() {
        return mBase.doesProfileHaveMultipleCustodians();
    }

    @Override
    public @LayoutRes int defaultPreferenceLayoutResource() {
        return mBase.defaultPreferenceLayoutResource();
    }

    /* Do not override the 'isPreferenceClickDisabledByPolicy' method in this class as this causes
     * the wrong version to be called for instances of `SingleCategoryManagedPreferenceDelegate`.
     * Refer to crbug/1380613 for more details. */
}
