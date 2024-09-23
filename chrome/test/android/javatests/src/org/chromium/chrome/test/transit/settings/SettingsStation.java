// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.settings;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.ActivityElement;
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
public class SettingsStation extends Station {
    private FragmentElement<MainSettings, SettingsActivity> mMainSettings;

    @Override
    public void declareElements(Elements.Builder elements) {
        ActivityElement<SettingsActivity> activityElement =
                elements.declareActivity(SettingsActivity.class);
        mMainSettings =
                elements.declareElement(new FragmentElement<>(MainSettings.class, activityElement));
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
