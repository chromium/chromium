// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.settings;

import org.chromium.base.test.transit.FragmentElement;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Transition;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsActivity;

/**
 * A Settings screen.
 *
 * @param <FragmentT> type of ChromeBaseSettingsFragment shown in this screen
 */
public class SettingsStation<FragmentT extends ChromeBaseSettingsFragment>
        extends Station<SettingsActivity> {
    public final FragmentElement<FragmentT, SettingsActivity> fragmentElement;

    public SettingsStation(Class<FragmentT> fragmentClass) {
        super(SettingsActivity.class);
        fragmentElement = declareElement(new FragmentElement<>(fragmentClass, mActivityElement));
    }

    public PreferenceFacility scrollToPref(String prefKey) {
        assertInPhase(Phase.ACTIVE);
        String title = fragmentElement.get().findPreference(prefKey).getTitle().toString();
        return enterFacilitySync(
                new PreferenceFacility(title),
                Transition.newOptions()
                        .withPossiblyAlreadyFulfilled()
                        .withRunTriggerOnUiThread()
                        .build(),
                () -> fragmentElement.get().scrollToPreference(prefKey));
    }
}
