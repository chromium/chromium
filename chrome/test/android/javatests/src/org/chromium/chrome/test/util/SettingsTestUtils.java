// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.app.Activity;

import androidx.fragment.app.Fragment;

import org.hamcrest.Matchers;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsHostFragment;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;

import java.util.concurrent.atomic.AtomicReference;

/**
 * Test helpers for Settings, which opens in a {@link SettingsActivity} on phones and in a browser
 * tab on tablets and desktop (see {@link org.chromium.chrome.browser.settings.SettingsInTab}).
 */
@NullMarked
public final class SettingsTestUtils {
    /** Opening settings in a tab loads a native page, which can take longer than the default. */
    private static final long SETTINGS_LAUNCH_TIMEOUT_MS = 10000L;

    private SettingsTestUtils() {}

    /**
     * Runs {@code trigger} and waits until a settings fragment of {@code fragmentClass} is shown,
     * either as the main fragment of a {@link SettingsActivity} or in a settings tab.
     *
     * @return the Activity hosting settings, to pass to {@link #closeSettings(Activity)}.
     */
    public static Activity waitForSettingsFragment(
            Runnable trigger, Class<? extends Fragment> fragmentClass) {
        trigger.run();
        AtomicReference<Activity> hostRef = new AtomicReference<>();
        CriteriaHelper.pollUiThread(
                () -> {
                    for (Activity activity : ApplicationStatus.getRunningActivities()) {
                        if (findSettingsFragment(activity, fragmentClass) != null) {
                            hostRef.set(activity);
                            return;
                        }
                    }
                    throw new CriteriaNotSatisfiedException(
                            "No settings showing " + fragmentClass.getSimpleName());
                },
                SETTINGS_LAUNCH_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        return hostRef.get();
    }

    /**
     * Returns the fragment of {@code fragmentClass} that settings hosted by {@code activity} is
     * showing, or null if {@code activity} does not host such a settings fragment.
     *
     * <p>In multi-column settings the main fragment is the detail pane, while MainSettings is the
     * header pane, so the children of MultiColumnSettings are searched as well.
     */
    public static <T extends Fragment> @Nullable T findSettingsFragment(
            Activity activity, Class<T> fragmentClass) {
        // getRunningActivities() can include activities from a previous test that are still being
        // torn down.
        if (activity.isFinishing() || activity.isDestroyed()) return null;
        Fragment mainFragment;
        Fragment multiColumnSettings;
        if (activity instanceof SettingsActivity settingsActivity) {
            mainFragment = settingsActivity.getMainFragment();
            multiColumnSettings = settingsActivity.getMultiColumnSettings();
        } else {
            SettingsHostFragment hostFragment = SettingsHostFragment.get(activity);
            if (hostFragment == null) return null;
            mainFragment = hostFragment.getMainFragment();
            multiColumnSettings = hostFragment.getMultiColumnSettings();
        }
        if (fragmentClass.isInstance(mainFragment)) return fragmentClass.cast(mainFragment);
        if (multiColumnSettings != null && multiColumnSettings.isAdded()) {
            for (Fragment child : multiColumnSettings.getChildFragmentManager().getFragments()) {
                if (fragmentClass.isInstance(child)) return fragmentClass.cast(child);
            }
        }
        return null;
    }

    /**
     * Closes settings hosted by {@code activity}, as returned by {@link #waitForSettingsFragment}:
     * finishes a {@link SettingsActivity}, or closes the settings tab of a {@link
     * ChromeTabbedActivity} and waits for another tab to become active.
     */
    public static void closeSettings(Activity activity) {
        if (activity instanceof SettingsActivity) {
            ApplicationTestUtils.finishActivity(activity);
            return;
        }
        assert activity instanceof ChromeTabbedActivity
                : "Settings are hosted by " + activity.getClass().getSimpleName();
        ChromeTabbedActivity cta = (ChromeTabbedActivity) activity;
        Tab settingsTab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Tab tab = cta.getActivityTab();
                            assert tab != null;
                            cta.getCurrentTabModel()
                                    .getTabRemover()
                                    .closeTabs(
                                            TabClosureParams.closeTab(tab).allowUndo(false).build(),
                                            /* allowDialog= */ false);
                            return tab;
                        });
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab activityTab = cta.getActivityTab();
                    Criteria.checkThat(activityTab, Matchers.notNullValue());
                    Criteria.checkThat(activityTab, Matchers.not(settingsTab));
                });
    }
}
