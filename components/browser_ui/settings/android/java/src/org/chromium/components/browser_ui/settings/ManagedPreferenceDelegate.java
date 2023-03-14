// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import androidx.annotation.LayoutRes;
import androidx.preference.Preference;

/**
 * A delegate that determines whether a Preference is managed by enterprise policy. This is used
 * in various Preference subclasses (e.g. ChromeSwitchPreference) to determine whether to show
 * an enterprise icon next to the Preference and whether to disable clicks on the Preference.
 *
 * An implementation of this delegate should override isPreferenceControlledByPolicy() and,
 * optionally, isPreferenceClickDisabledByPolicy(). Example:
 *
 *   class RocketManagedPreferenceDelegate extends ManagedPreferenceDelegate {
 *       @Override
 *       public boolean isPreferenceControlledByPolicy(Preference preference) {
 *           if ("enable_rockets".equals(preference.getKey())) {
 *               return RocketUtils.isEnableRocketsManaged();
 *           }
 *           return false;
 *       }
 *   }
 *
 *   ChromeSwitchPreference enableRocketsPref = ...;
 *   enableRocketsPref.setManagedPreferenceDelegate(new RocketManagedPreferenceDelegate());
 */
public interface ManagedPreferenceDelegate {
    /**
     * Returns whether the given Preference is controlled by an enterprise policy.
     * @param preference the {@link Preference} under consideration.
     * @return whether the given Preference is controlled by an enterprise policy.
     */
    boolean isPreferenceControlledByPolicy(Preference preference);

    /**
     * Returns whether the given Preference is controlled by the supervised user's custodian.
     * @param preference the {@link Preference} under consideration.
     * @return whether the given Preference is controlled by the supervised user's custodian.
     */
    boolean isPreferenceControlledByCustodian(Preference preference);

    /**
     * Returns whether the current Profile is managed by multiple custodians.
     *
     * This is used to control messaging when a Preference is managed by a custodian(s).
     */
    boolean doesProfileHaveMultipleCustodians();

    /**
     * Returns the layout resource to be used by default for preferences that can be managed, when
     * a custom layout is not defined. Return value 0 can be used to indicate that Android's default
     * preference layout should be used.
     *
     * <p>Embedders should define the default behavior that should apply to all preferences that
     * can be managed. This way, only embedders that do require custom layouts to pay the price in
     * binary size increase due to dependencies.
     */
    @LayoutRes
    int defaultPreferenceLayoutResource();

    /**
     * Returns whether clicking on the given Preference is disabled. The default
     * implementation just returns whether the preference is not modifiable by the user.
     * However, some preferences that are controlled by policy may still be clicked to show an
     * informational subscreen, in which case this method needs a custom implementation.
     */
    default boolean isPreferenceClickDisabled(Preference preference) {
        return isPreferenceControlledByPolicy(preference)
                || isPreferenceControlledByCustodian(preference);
    }
}
