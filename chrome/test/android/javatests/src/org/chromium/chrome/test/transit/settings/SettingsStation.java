// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.settings;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.FragmentElement;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Transition;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivity;

/**
 * The initial and main Settings screen.
 *
 * <p>TODO(crbug.com/328277614): This is a stub; add more elements and methods.
 */
public class SettingsStation extends Station<SettingsActivity> {
    private FragmentElement<MainSettings, SettingsActivity> mMainSettings;

    public SettingsStation() {
        super(SettingsActivity.class);
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);
        mMainSettings =
                elements.declareElement(
                        new FragmentElement<>(MainSettings.class, mActivityElement));
    }

    public PreferenceFacility scrollToPref(String prefKey) {
        assertSuppliersCanBeUsed();
        String title = mMainSettings.get().findPreference(prefKey).getTitle().toString();
        return enterFacilitySync(
                new PreferenceFacility(title),
                Transition.newOptions().withPossiblyAlreadyFulfilled().build(),
                () ->
                        ThreadUtils.runOnUiThreadBlocking(
                                () -> mMainSettings.get().scrollToPreference(prefKey)));
    }
}
