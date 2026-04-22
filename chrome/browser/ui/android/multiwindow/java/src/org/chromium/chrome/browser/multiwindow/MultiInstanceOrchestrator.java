// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.os.Bundle;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

import java.util.List;

/**
 * Class that orchestrates and provides for common tasks applicable to all windows in a multi-window
 * environment.
 */
@NullMarked
public interface MultiInstanceOrchestrator {
    /**
     * Tracks {@link Activity} / {@link MultiInstanceManager} associations when an activity-scoped
     * {@link MultiInstanceManager} is fully initialized. This is expected to be invoked during
     * {@link MultiInstanceManager#initialize(int, int, int)}.
     *
     * @param activity The {@link Activity} that was started.
     * @param multiInstanceManager The {@link MultiInstanceManager} created for {@code activity}.
     */
    void onInitialize(Activity activity, MultiInstanceManager multiInstanceManager);

    /**
     * Creates a new ChromeTabbedActivity instance.
     *
     * @param sourceActivity The activity used to launch the intent.
     * @param isIncognito Whether the new window should be in incognito mode.
     * @param additionalIntentExtras An optional bundle specifying extras to add to the intent used
     *     to create the new window.
     * @param startActivityOptions An optional bundle that will be used to start the activity.
     * @param source The new window creation source used for metrics.
     * @return true if the window was successfully created, false otherwise.
     */
    boolean createNewWindow(
            Activity sourceActivity,
            boolean isIncognito,
            @Nullable Bundle additionalIntentExtras,
            @Nullable Bundle startActivityOptions,
            @NewWindowAppSource int source);

    /**
     * Creates a new ChromeTabbedActivity instance from an existing {@link WebContents}.
     *
     * @param sourceActivity The activity used to launch the intent.
     * @param profile The {@link Profile} associated with the web contents.
     * @param webContents The {@link WebContents} to use in the new window.
     * @param additionalIntentExtras An optional bundle specifying extras to add to the intent used
     *     to create the new window.
     * @param startActivityOptions An optional bundle that will be used to start the activity.
     * @param source The new window creation source used for metrics.
     * @return true if the window was successfully created, false otherwise.
     *     <p>Note: Do not use the provided WebContents after calling this function. This function
     *     will take ownership of the provided WebContents, potentially destroying it.
     */
    boolean createNewWindowFromWebContents(
            Activity sourceActivity,
            Profile profile,
            WebContents webContents,
            @Nullable Bundle additionalIntentExtras,
            @Nullable Bundle startActivityOptions,
            @NewWindowAppSource int source);

    /**
     * Moves the specified tabs to a new ChromeTabbedActivity instance.
     *
     * @param tabs The list of tabs to move.
     * @param finalizeCallback A runnable that will be invoked after the tabs have finished
     *     reparenting to the new window.
     * @param source The new window creation source used for metrics.
     */
    void moveTabsToNewWindow(
            List<Tab> tabs, @Nullable Runnable finalizeCallback, @NewWindowAppSource int source);

    /**
     * Moves the specified tabs to the specified ChromeTabbedActivity instance. This accepts inputs
     * to determine the position of the moved tabs in the destination window. The operation will
     * fail if the instance is not found.
     *
     * @param destWindowId The id of the destination window.
     * @param tabs The list of tabs to move.
     * @param destTabIndex The tab index in the destination window where the tabs will be
     *     positioned. This will be ignored if {@code destGroupTabId} is set. To use the default tab
     *     index, set this to {@code TabList.INVALID_TAB_INDEX}.
     * @param destGroupTabId The id of the tab in the destination tab group, if the tabs need to be
     *     moved to a specific tab group in the destination window. The tabs will be added to the
     *     end of the destination tab group. A tab with this id must exist in the destination
     *     window, otherwise this operation will fail. If there is no tab group to move the
     *     specified tabs to, set this to {@code TabList.INVALID_TAB_INDEX}.
     * @param bringToFront Whether the destination window should be brought to the front.
     */
    void moveTabsToWindowByIdChecked(
            int destWindowId,
            List<Tab> tabs,
            int destTabIndex,
            int destGroupTabId,
            boolean bringToFront);

    /**
     * Moves the specified tabs to a selected ChromeTabbedActivity instance. If there is no other
     * eligible window currently, tabs will be moved to a new window. Otherwise, the user will be
     * presented with a UI to select a window to move the tabs to.
     *
     * @param tabs The list of tabs to move.
     * @param source The new window creation source used for metrics.
     */
    void moveTabsToOtherWindow(List<Tab> tabs, @NewWindowAppSource int source);

    /**
     * Moves the specified tab group to a new ChromeTabbedActivity instance.
     *
     * @param tabGroupMetadata The {@link TabGroupMetadata} describing the tab group being moved.
     * @param source The new window creation source used for metrics.
     */
    void moveTabGroupToNewWindow(TabGroupMetadata tabGroupMetadata, @NewWindowAppSource int source);

    /**
     * Moves a tab group to the specified position in the specified ChromeTabbedActivity instance.
     * The operation will fail if the instance is not found.
     *
     * @param destWindowId The id of the destination window.
     * @param tabGroupMetadata The {@link TabGroupMetadata} describing the tab group being moved.
     * @param destTabIndex The tab index in the destination window where the tab group will be
     *     positioned. To use the default tab index, set this to {@code TabList.INVALID_TAB_INDEX}.
     * @param bringToFront Whether the destination window should be brought to the front.
     */
    void moveTabGroupToWindowByIdChecked(
            int destWindowId,
            TabGroupMetadata tabGroupMetadata,
            int destTabIndex,
            boolean bringToFront);

    /**
     * Moves the specified tab group to a selected ChromeTabbedActivity instance. If there are no
     * other eligible windows currently, the tab group will be moved to a new window. Otherwise, the
     * user will be presented with a UI to select a window to move the tab group to.
     *
     * @param tabGroupMetadata The {@link TabGroupMetadata} describing the tab group being moved.
     * @param source The new window creation source used for metrics.
     */
    void moveTabGroupToOtherWindow(
            TabGroupMetadata tabGroupMetadata, @NewWindowAppSource int source);

    /**
     * Opens a URL in an existing window or a new window with profile type determined by {@code
     * isIncognito}.
     *
     * @param sourceActivity The activity initiating the url launch request.
     * @param loadUrlParams The {@link LoadUrlParams} describing the url to open.
     * @param parentTabId The ID of the parent tab, or {@link Tab#INVALID_TAB_ID}.
     * @param preferNew Whether we should prioritize launching the tab in a new window.
     * @param isIncognito Whether the target window should be an incognito window when supported.
     * @return {@code true} if the url launch request was successful, {@code false} otherwise.
     */
    boolean openUrlInOtherWindow(
            Activity sourceActivity,
            LoadUrlParams loadUrlParams,
            int parentTabId,
            boolean preferNew,
            boolean isIncognito);
}
