// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.UiUtils.NameWindowDialogSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.content_public.browser.LoadUrlParams;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Manages multi-instance mode for an associated activity. After construction, call {@link
 * #isStartedUpCorrectly(int)} to validate that the owning Activity should be allowed to finish
 * starting up.
 */
@NullMarked
public abstract class MultiInstanceManager {
    public static final int INVALID_TASK_ID = -1; // Defined in android.app.ActivityTaskManager.
    public static final String NEW_WINDOW_APP_SOURCE_HISTOGRAM =
            "Android.MultiWindowMode.NewWindow.AppSource";

    @VisibleForTesting
    static final String CLOSE_WINDOW_APP_SOURCE_HISTOGRAM =
            "Android.MultiWindowMode.CloseWindow.AppSource";

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @IntDef({
        NewWindowAppSource.OTHER,
        NewWindowAppSource.MENU,
        NewWindowAppSource.WINDOW_MANAGER,
        NewWindowAppSource.KEYBOARD_SHORTCUT
    })
    public @interface NewWindowAppSource {
        int OTHER = 0;
        int MENU = 1;
        int WINDOW_MANAGER = 2;
        int KEYBOARD_SHORTCUT = 3;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 4;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        CloseWindowAppSource.OTHER,
        CloseWindowAppSource.WINDOW_MANAGER,
        CloseWindowAppSource.RETENTION_PERIOD_EXPIRATION,
        CloseWindowAppSource.NO_TABS_IN_WINDOW
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CloseWindowAppSource {
        int OTHER = 0;
        int WINDOW_MANAGER = 1;
        int RETENTION_PERIOD_EXPIRATION = 2;
        int NO_TABS_IN_WINDOW = 3;

        // Update enums.xml when updating these values.
        int NUM_ENTRIES = 4;
    }

    /** Should be called when multi-instance mode is started. */
    public static void onMultiInstanceModeStarted() {
        // When a second instance is created, the merged instance task id should be cleared.
        setMergedInstanceTaskId(0);
    }

    /** The class of the activity will do merge on start up. */
    protected static @Nullable Class sActivityTypePendingMergeOnStartup;

    /** The task id of the activity that tabs were merged into. */
    protected static int sMergedInstanceTaskId;

    protected static List<Integer> sTestDisplayIds = new ArrayList<>();

    /** The type of tab/profile the activity supports. */
    @IntDef({
        SupportedProfileType.UNSET,
        SupportedProfileType.REGULAR,
        SupportedProfileType.OFF_THE_RECORD,
        SupportedProfileType.MIXED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface SupportedProfileType {
        int UNSET = 0;
        int REGULAR = 1;
        int OFF_THE_RECORD = 2;
        int MIXED = 3;
    }

    /**
     * Called during activity startup to check whether the activity is recreated because the
     * secondary display is removed.
     *
     * @return True if the activity is recreated after a display is removed. Should consider merging
     *     tabs.
     */
    public static boolean shouldMergeOnStartup(Activity activity) {
        return sActivityTypePendingMergeOnStartup != null
                && sActivityTypePendingMergeOnStartup.equals(activity.getClass());
    }

    /**
     * Called after {@link #shouldMergeOnStartup(Activity)} to indicate merge has started, so there
     * is no merge on following recreate.
     */
    public static void mergedOnStartup() {
        sActivityTypePendingMergeOnStartup = null;
    }

    protected static void setMergedInstanceTaskId(int mergedInstanceTaskId) {
        sMergedInstanceTaskId = mergedInstanceTaskId;
    }

    /**
     * Called during activity startup to check whether this instance of the MultiInstanceManager is
     * associated with an activity task ID that should be started up.
     *
     * @return True if the activity should proceed with startup. False otherwise.
     */
    public abstract boolean isStartedUpCorrectly(int activityTaskId);

    /**
     * Creates a new {@link Intent} for a new instance of the main Chrome window (Task).
     *
     * <p>The root {@link Activity} of the new Chrome window depends on the implementation. It can
     * be either {@code ChromeTabbedActivity} or {@code ChromeTabbedActivity2}.
     *
     * <p>The intended use cases of this method:
     *
     * <ul>
     *   <li>The caller doesn't (need to) know the specifics of the {@link Intent}, such as flags,
     *       Extras, the target {@link Activity}, the new window's instance ID, etc. An example of
     *       this is the "open new window" option in the app menu.
     *   <li>The caller is in a modularized target and can't depend on code at the "glue" layer,
     *       such as {@link MultiWindowUtils#createNewWindowIntent}. In this case, the caller should
     *       inject {@link MultiInstanceManager} at the "glue" layer, then use it in the caller's
     *       internal logic to create the {@link Intent}.
     * </ul>
     *
     * @param isIncognito Whether the new window should be in the incognito mode.
     * @return The new {@link Intent} as described above, or {@code null} if the new window cannot
     *     be created.
     */
    public abstract @Nullable Intent createNewWindowIntent(boolean isIncognito);

    /**
     * Merges tabs from a second ChromeTabbedActivity instance if necessary and calls
     * finishAndRemoveTask() on the other activity.
     */
    @VisibleForTesting
    public abstract void maybeMergeTabs();

    /**
     * Open a new instance of the ChromeTabbedActivity window and move the specified tabs from
     * existing instance to the new one.
     *
     * @param tabs Tabs that are to be moved to a new Chrome instance.
     * @param source The new window creation source used for metrics.
     */
    public void moveTabsToNewWindow(List<Tab> tabs, @NewWindowAppSource int source) {
        // Not implemented
    }

    /**
     * Open a new instance of the ChromeTabbedActivity window and move the specified tab group from
     * existing instance to the new one.
     *
     * @param tabGroupMetadata The object containing the metadata of the tab group.
     * @param source The new window creation source used for metrics.
     */
    public void moveTabGroupToNewWindow(
            TabGroupMetadata tabGroupMetadata, @NewWindowAppSource int source) {
        // Not implemented
    }

    /**
     * Move the specified tabs to the current instance of the ChromeTabbedActivity window.
     *
     * @param activity Activity of the Chrome Window in which the tab is to be moved.
     * @param tabs The list of tabs that is to be moved to the current instance.
     * @param atIndex Tab position index in the destination window instance.
     */
    public void moveTabsToWindow(@Nullable Activity activity, List<Tab> tabs, int atIndex) {
        // Not implemented
    }

    /**
     * Move the specified tabs to the specified instance of the ChromeTabbedActivity window.
     *
     * @param info {@link InstanceInfo} describing the destination window.
     * @param tabs The list of tabs that is to be moved to the current instance.
     * @param atIndex Tab position index in the destination window instance.
     * @param source The new window creation source used for metrics.
     */
    public void moveTabsToWindow(
            InstanceInfo info, List<Tab> tabs, int atIndex, @NewWindowAppSource int source) {
        // Not implemented
    }

    /**
     * Move the specified tabs to the specified instance of the ChromeTabbedActivity window and
     * merge with the destination tab group. The tabs are added to the end of the destination tab
     * group. If the activity from {@code info} does not exist, this will not create a new window.
     *
     * @param info {@link InstanceInfo} describing the destination window.
     * @param tabs The list of ungrouped tabs that is to be moved to the current instance.
     * @param destTabId The id of the tab in the destination tab group. The tab with this ID must
     *     exist in the destination window, otherwise this operation will fail.
     */
    public void moveTabsToWindowAndMergeToDest(InstanceInfo info, List<Tab> tabs, int destTabId) {
        // Not implemented
    }

    /**
     * Move an entire tab group to the current instance of the ChromeTabbedActivity window.
     *
     * @param activity Activity of the Chrome Window in which the tab group is to be moved.
     * @param tabGroupMetadata The object containing the metadata of the tab group.
     * @param atIndex Tab position index in the destination window instance.
     */
    public void moveTabGroupToWindow(
            @Nullable Activity activity, TabGroupMetadata tabGroupMetadata, int atIndex) {
        // Not implemented
    }

    /**
     * Move an entire tab group to the specified instance of the ChromeTabbedActivity window.
     *
     * @param info {@link InstanceInfo} describing the destination window.
     * @param tabGroupMetadata The object containing the metadata of the tab group.
     * @param atIndex Tab position index in the destination window instance.
     * @param source The new window creation source used for metrics.
     */
    public void moveTabGroupToWindow(
            InstanceInfo info,
            TabGroupMetadata tabGroupMetadata,
            int atIndex,
            @NewWindowAppSource int source) {
        // Not implemented
    }

    /**
     * If there's only one window currently, moves {@param tabs} to a new window. Otherwise, opens a
     * dialog to select which window to move {@param tabs} to.
     *
     * @param tabs The list of tabs to move.
     * @param source The new window creation source used for metrics.
     */
    public void moveTabsToOtherWindow(List<Tab> tabs, @NewWindowAppSource int source) {
        // Not implemented
    }

    /**
     * Opens a URL in another existing window or a new window.
     *
     * @param loadUrlParams The url to open.
     * @param parentTabId The ID of the parent tab.
     * @param preferNew Whether we should prioritize launching the tab in a new window.
     * @param instanceType The {@link PersistedInstanceType} that will be used to determine the type
     *     of window the URL can be opened in.
     */
    public void openUrlInOtherWindow(
            LoadUrlParams loadUrlParams,
            int parentTabId,
            boolean preferNew,
            @PersistedInstanceType int instanceType) {
        // not implemented
    }

    /**
     * If there's only one window currently, moves the matching group to a new window. Otherwise,
     * opens a dialog to select which window to move the matching group to.
     *
     * @param tabGroupMetadata The metadata for the group to move.
     * @param source The new window creation source used for metrics.
     */
    public void moveTabGroupToOtherWindow(
            TabGroupMetadata tabGroupMetadata, @NewWindowAppSource int source) {
        // Not implemented
    }

    /**
     * @return List of {@link InstanceInfo} structs for an activity that can be switched to, or
     *     newly launched.
     */
    public List<InstanceInfo> getInstanceInfo() {
        return getInstanceInfo(PersistedInstanceType.ANY);
    }

    /**
     * @return List of {@link InstanceInfo} structs with {@link PersistedInstanceType} {@param type}
     *     for an activity that can be switched to, or newly launched.
     */
    public List<InstanceInfo> getInstanceInfo(@PersistedInstanceType int type) {
        return Collections.emptyList();
    }

    /**
     * Assigned an ID for the current activity instance.
     *
     * @param windowId Instance ID explicitly given for assignment.
     * @param taskId Task ID of the activity.
     * @param preferNew Boolean indicating a fresh new instance is preferred over the one that will
     *     load previous tab files from disk.
     * @param profileType The type of tab/profile the activity supports.
     */
    public abstract Pair<Integer, Integer> allocInstanceId(
            int windowId, int taskId, boolean preferNew, @SupportedProfileType int profileType);

    /**
     * Initialize the manager with the allocated instance ID.
     *
     * @param instanceId Instance ID of the activity.
     * @param taskId Task ID of the activity.
     */
    public void initialize(int instanceId, int taskId) {}

    /** Perform initialization tasks for the manager after the tab state is initialized. */
    public void onTabStateInitialized() {}

    /**
     * @return True if tab model merging for Android N+ is enabled.
     */
    public boolean isTabModelMergingEnabled() {
        return !CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING);
    }

    /**
     * @return InstanceId for current instance.
     */
    public abstract int getCurrentInstanceId();

    /**
     * Close a Chrome window instance only if it contains no open tabs including incognito ones.
     *
     * @param instanceId Instance id of the Chrome window that needs to be closed.
     * @return {@code true} if the window was closed, {@code false} otherwise.
     */
    public boolean closeChromeWindowIfEmpty(int instanceId) {
        return false;
    }

    /**
     * Close the window associated with a given task / activity. This will permanently and
     * irreversibly delete persisted instance and tab state data.
     *
     * @param instanceId ID of the activity instance.
     * @param source The {@link CloseWindowAppSource} that reflects the source of instance closure.
     */
    public void closeWindow(int instanceId, @CloseWindowAppSource int source) {}

    /**
     * Intended to be called on initialization. If there's only one window at the moment that has
     * tabs stored for it, we then know that any tabs and groups that sync knows of are not in other
     * windows, and their local ids should be cleared out.
     *
     * @param selector The root entry point to tab model objects. Does not necessarily have to be
     *     done initializing.
     */
    public void cleanupSyncedTabGroupsIfOnlyInstance(TabModelSelector selector) {
        // Not implemented
    }

    /**
     * Shows a message to notify the user when excess of {@link MultiWindowUtils#getMaxInstances()}
     * running activities have been finished after an instance limit downgrade causing existence of
     * more active instances than the instance limit.
     *
     * @param messageDispatcher The {@link MessageDispatcher} to enqueue the instance restoration
     *     message.
     * @return {@code true} if the instance restoration message was shown, {@code false} otherwise.
     */
    public boolean showInstanceRestorationMessage(@Nullable MessageDispatcher messageDispatcher) {
        return false;
    }

    /**
     * Shows a message to notify the user that a new window cannot be created because {@link
     * MultiWindowUtils#getMaxInstances()} activities already exist.
     *
     * @param messageDispatcher The {@link MessageDispatcher} to enqueue the instance limit message.
     */
    public void showInstanceCreationLimitMessage(@Nullable MessageDispatcher messageDispatcher) {
        // Not implemented
    }

    /**
     * Shows a dialog to name the current window.
     *
     * @param source The {@link NameWindowDialogSource} that tracks the caller of this method.
     */
    public void showNameWindowDialog(@NameWindowDialogSource int source) {
        // Not implemented
    }

    public abstract void setCurrentDisplayIdForTesting(int displayId);

    public abstract DisplayManager.@Nullable DisplayListener getDisplayListenerForTesting();

    @VisibleForTesting
    public static void setTestDisplayIds(List<Integer> testDisplayIds) {
        sTestDisplayIds = testDisplayIds;
    }

    public abstract @Nullable TabModelSelectorTabModelObserver getTabModelObserverForTesting();

    public abstract void setTabModelObserverForTesting(
            TabModelSelectorTabModelObserver tabModelObserver);

    // The instance types are defined as bit flags, so they can be or-ed to reflect
    // more than one value. Or-ed values should be validated at points of access.
    @IntDef(
            flag = true,
            value = {
                PersistedInstanceType.ANY,
                PersistedInstanceType.ACTIVE,
                PersistedInstanceType.INACTIVE,
                PersistedInstanceType.OFF_THE_RECORD,
                PersistedInstanceType.REGULAR
            })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PersistedInstanceType {
        // Represents any instance not bound by any specific type.
        int ANY = 0;

        // Represents an active instance that is associated with a live task.
        int ACTIVE = 1 << 0;

        // Represents an inactive instance that is not associated with a live task.
        int INACTIVE = 1 << 1;

        // Represents an instance for an incognito-only window.
        int OFF_THE_RECORD = 1 << 2;

        // Represents an instance for a regular window that does not hold incognito tabs.
        int REGULAR = 1 << 3;
    }
}
