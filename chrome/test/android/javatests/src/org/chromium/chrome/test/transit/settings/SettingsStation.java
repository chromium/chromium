// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.settings;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Elements;
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
    private final Class<FragmentT> mFragmentClass;
    private FragmentElement<FragmentT, SettingsActivity> mFragmentElement;

    public SettingsStation(Class<FragmentT> fragmentClass) {
        super(SettingsActivity.class);
        mFragmentClass = fragmentClass;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);
        mFragmentElement =
                elements.declareElement(new FragmentElement<>(mFragmentClass, mActivityElement));
    }

    public PreferenceFacility scrollToPref(String prefKey) {
        assertSuppliersCanBeUsed();
        String title = mFragmentElement.get().findPreference(prefKey).getTitle().toString();
        return enterFacilitySync(
                new PreferenceFacility(title),
                Transition.newOptions().withPossiblyAlreadyFulfilled().build(),
                () ->
                        ThreadUtils.runOnUiThreadBlocking(
                                () -> mFragmentElement.get().scrollToPreference(prefKey)));
    }
}
