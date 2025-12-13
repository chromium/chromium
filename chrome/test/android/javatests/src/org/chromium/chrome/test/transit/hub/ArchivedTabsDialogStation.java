// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsDialogCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabArchiveSettingsFragment;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeActivityTabModelBoundStation;
import org.chromium.chrome.test.transit.settings.SettingsStation;

/** The station for the archived tabs dialog. */
public class ArchivedTabsDialogStation
        extends ChromeActivityTabModelBoundStation<ChromeTabbedActivity> {
    private final TabArchiveSettings mTabArchiveSettings;

    public ViewElement<View> dialogElement;
    public ViewElement<RecyclerView> recyclerViewElement;
    public ViewElement<View> closeButtonElement;
    public ViewElement<View> iphElement;

    public ArchivedTabsDialogStation() {
        super(ChromeTabbedActivity.class, /* isIncognito= */ false);
        mTabArchiveSettings =
                runOnUiThreadBlocking(
                        () -> new TabArchiveSettings(ChromeSharedPreferences.getInstance()));
        dialogElement = declareView(withId(R.id.archived_tabs_dialog));
        recyclerViewElement =
                declareView(
                        dialogElement.descendant(
                                RecyclerView.class, withId(R.id.tab_list_recycler_view)));
        closeButtonElement = declareView(withContentDescription("Hide inactive tabs dialog"));
        if (mTabArchiveSettings.shouldShowDialogIph()) {
            declareElementFactory(
                    mActivityElement,
                    delayedElements -> {
                        iphElement =
                                delayedElements.declareView(
                                        recyclerViewElement.descendant(
                                                withText(getIphDescription())));
                    });
        }
    }

    public SettingsStation<TabArchiveSettingsFragment> openSettings(Runnable settingsTrigger) {
        assert mTabArchiveSettings.shouldShowDialogIph();
        return runTo(settingsTrigger)
                .arriveAt(new SettingsStation<>(TabArchiveSettingsFragment.class));
    }

    public RegularTabSwitcherStation closeDialog() {
        return closeButtonElement
                .clickTo()
                .arriveAt(RegularTabSwitcherStation.from(tabModelSelectorElement.value()));
    }

    // Private functions.

    private String getIphDescription() {
        return String.valueOf(
                ArchivedTabsDialogCoordinator.getIphDescription(
                        getActivity(), mTabArchiveSettings));
    }
}
