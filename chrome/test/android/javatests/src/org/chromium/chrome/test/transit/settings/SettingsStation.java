// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.settings;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.view.View;

import org.chromium.base.test.transit.FragmentElement;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsHostFragment;
import org.chromium.chrome.browser.settings.SettingsInTab;

/**
 * A Settings screen.
 *
 * @param <FragmentT> type of ChromeBaseSettingsFragment shown in this screen
 */
public class SettingsStation<FragmentT extends ChromeBaseSettingsFragment>
        extends Station<ChromeBaseAppCompatActivity> {
    public final FragmentElement<FragmentT, ChromeBaseAppCompatActivity> fragmentElement;
    public @Nullable ViewElement<View> searchBoxElement;

    @SuppressWarnings("unchecked")
    public SettingsStation(Class<FragmentT> fragmentClass) {
        super(
                (Class<ChromeBaseAppCompatActivity>)
                        (SettingsInTab.shouldOpenSettingsInTab()
                                ? ChromeBaseAppCompatActivity.class
                                : SettingsActivity.class));
        if (SettingsInTab.shouldOpenSettingsInTab()) {
            mActivityElement.allowSubclasses();
        }
        fragmentElement = declareElement(new FragmentElement<>(fragmentClass, mActivityElement));

        // SettingsSearchCoordinator shows the search box in the app bar in a posted task, which
        // resizes the settings list. Declare the search box ViewElement (or NoView) so that the
        // station is only entered after the search box visibility and layout have settled.
        declareElementFactory(
                fragmentElement,
                delayedElements -> {
                    SettingsHostFragment hostFragment =
                            SettingsHostFragment.get(fragmentElement.value());
                    boolean isTwoColumn =
                            hostFragment != null
                                    ? hostFragment.isTwoColumnSettingsVisible()
                                    : (mActivityElement.value()
                                                    instanceof SettingsActivity settingsActivity
                                            && settingsActivity.isTwoColumnSettingsVisible());
                    if (fragmentClass == MainSettings.class || isTwoColumn) {
                        searchBoxElement = delayedElements.declareView(withId(R.id.search_box));
                    } else {
                        delayedElements.declareNoView(withId(R.id.search_box));
                    }
                });
    }

    public PreferenceFacility scrollToPref(String prefKey) {
        assertInPhase(Phase.ACTIVE);
        FragmentT fragment = fragmentElement.value();
        String title = fragment.findPreference(prefKey).getTitle().toString();
        return runOnUiThreadTo(() -> fragment.scrollToPreference(prefKey))
                .withPossiblyAlreadyFulfilled()
                .enterFacility(new PreferenceFacility(title));
    }
}
