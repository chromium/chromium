// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.settings;

import static org.chromium.base.test.transit.Condition.fulfilled;
import static org.chromium.base.test.transit.Condition.notFulfilled;
import static org.chromium.base.test.transit.SimpleConditions.uiThreadCondition;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.test.transit.FragmentElement;
import org.chromium.base.test.transit.Station;
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
        // Wait until the fragment's RecyclerView has finished laying out its items (including the
        // adapter re-inflation triggered by SettingsContainmentHelper on the initial layout pass)
        // so that subsequent actions such as scrollToPref() can run unconditionally.
        declareEnterCondition(
                uiThreadCondition(
                        "Fragment RecyclerView is laid out",
                        fragmentElement,
                        fragment -> {
                            RecyclerView listView = fragment.getListView();
                            if (listView == null || !listView.isShown()) {
                                return notFulfilled("RecyclerView is not shown");
                            }
                            if (listView.isLayoutRequested()
                                    || listView.hasPendingAdapterUpdates()) {
                                return notFulfilled(
                                        "RecyclerView layout is pending (isLayoutRequested=%b,"
                                                + " hasPendingAdapterUpdates=%b)",
                                        listView.isLayoutRequested(),
                                        listView.hasPendingAdapterUpdates());
                            }
                            if (listView.getAdapter() != null
                                    && listView.getAdapter().getItemCount() > 0
                                    && listView.getChildCount() == 0) {
                                return notFulfilled("RecyclerView has no child views laid out yet");
                            }
                            return fulfilled();
                        }));
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
