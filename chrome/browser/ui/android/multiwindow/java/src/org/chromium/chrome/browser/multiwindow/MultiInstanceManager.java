// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.hardware.display.DisplayManager;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.UiUtils.NameWindowDialogSource;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;

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
    public static final int INVALID_WINDOW_ID = -1;
    public static final String NEW_WINDOW_APP_SOURCE_HISTOGRAM =
            "Android.MultiWindowMode.NewWindow.AppSource4";

    @VisibleForTesting
    static final String CLOSE_WINDOW_APP_SOURCE_HISTOGRAM =
            "Android.MultiWindowMode.CloseWindow.AppSource3";

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused. If none of the existing values are suitable for a feature that
    // creates a new window, update the enum.
    // LINT.IfChange(NewWindowAppSource)
    @IntDef({
        NewWindowAppSource.UNKNOWN,
        NewWindowAppSource.MENU,
        NewWindowAppSource.WINDOW_MANAGER,
        NewWindowAppSource.KEYBOARD_SHORTCUT,
        NewWindowAppSource.RECENT_TABS,
        NewWindowAppSource.DRAG_DROP_LAUNCHER,
        NewWindowAppSource.TAB_REPARENTING_TO_INSTANCE_WITH_NO_ACTIVITY,
        NewWindowAppSource.URL_LAUNCH,
        NewWindowAppSource.EXTERNAL_NAVIGATION,
        NewWindowAppSource.DEV_TOOLS,
        NewWindowAppSource.BROWSER_WINDOW_CREATOR,
        NewWindowAppSource.ANDROID_S_UPDATE,
        NewWindowAppSource.CRASH_RECOVERY
    })
    public @interface NewWindowAppSource {
        int UNKNOWN = 0;
        int MENU = 1;
        int WINDOW_MANAGER = 2;
        int KEYBOARD_SHORTCUT = 3;
        int RECENT_TABS = 4;
        int DRAG_DROP_LAUNCHER = 5;
        int TAB_REPARENTING_TO_INSTANCE_WITH_NO_ACTIVITY = 6;
        int URL_LAUNCH = 7;
        int EXTERNAL_NAVIGATION = 8;
        int DEV_TOOLS = 9;
        int BROWSER_WINDOW_CREATOR = 10;
        int ANDROID_S_UPDATE = 11;
        int CRASH_RECOVERY = 12;
        int NUM_ENTRIES = 13;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml)

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // LINT.IfChange(CloseWindowAppSource)
    @IntDef({
        CloseWindowAppSource.OTHER,
        CloseWindowAppSource.WINDOW_MANAGER,
        CloseWindowAppSource.RETENTION_PERIOD_EXPIRATION,
        CloseWindowAppSource.NO_TABS_IN_WINDOW,
        CloseWindowAppSource.RECENT_TABS,
        CloseWindowAppSource.RECENTLY_CLOSED_LIMIT_EXCEEDED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CloseWindowAppSource {
        int OTHER = 0;
        int WINDOW_MANAGER = 1;
        int RETENTION_PERIOD_EXPIRATION = 2;
        int NO_TABS_IN_WINDOW = 3;
        int RECENT_TABS = 4;
        int RECENTLY_CLOSED_LIMIT_EXCEEDED = 5;
        int NUM_ENTRIES = 6;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml)

    @IntDef({
        InstanceAllocationType.DEFAULT,
        InstanceAllocationType.EXISTING_INSTANCE_UNMAPPED_TASK,
        InstanceAllocationType.EXISTING_INSTANCE_MAPPED_TASK,
        InstanceAllocationType.PREFER_NEW_INSTANCE_NEW_TASK,
        InstanceAllocationType.PREFER_NEW_INVALID_INSTANCE,
        InstanceAllocationType.NEW_INSTANCE_NEW_TASK,
        InstanceAllocationType.EXISTING_INSTANCE_NEW_TASK,
        InstanceAllocationType.INVALID_INSTANCE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface InstanceAllocationType {
        int DEFAULT = 0;
        int EXISTING_INSTANCE_UNMAPPED_TASK = 1;
        int EXISTING_INSTANCE_MAPPED_TASK = 2;
        int PREFER_NEW_INSTANCE_NEW_TASK = 3;
        int PREFER_NEW_INVALID_INSTANCE = 4;
        int NEW_INSTANCE_NEW_TASK = 5;
        int EXISTING_INSTANCE_NEW_TASK = 6;
        int INVALID_INSTANCE = 7;
    }

    /** A class that holds information about an allocated instance ID. */
    public static class AllocatedIdInfo {
        public final int instanceId;
        public final @InstanceAllocationType int allocationType;
        public final @SupportedProfileType int profileType;

        public AllocatedIdInfo(
                int instanceId,
                @InstanceAllocationType int allocationType,
                @SupportedProfileType int profileType) {
            this.instanceId = instanceId;
            this.allocationType = allocationType;
            this.profileType = profileType;
        }
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
     * Merges tabs from a second ChromeTabbedActivity instance if necessary and calls
     * finishAndRemoveTask() on the other activity.
     */
    @VisibleForTesting
    public abstract void maybeMergeTabs();

    /**
     * @return A list of {@link InstanceInfo} structs for the specified {@link
     *     PersistedInstanceType}. This excludes unusable instances that are marked for deletion.
     */
    public List<InstanceInfo> getInstanceInfo(@PersistedInstanceType int type) {
        return Collections.emptyList();
    }

    /**
     * @return A list of {@link InstanceInfo} structs for inactive instances, including currently
     *     unusable instances that are marked for deletion. This excludes {@link
     *     PersistedInstanceType#OFF_THE_RECORD} type instances.
     */
    public List<InstanceInfo> getRecentlyClosedInstances() {
        return Collections.emptyList();
    }

    /**
     * Assigned an ID for the current activity instance.
     *
     * @param windowId Instance ID explicitly given for assignment.
     * @param taskId Task ID of the activity.
     * @param preferNew Boolean indicating a fresh new instance is preferred over the one that will
     *     load previous tab files from disk.
     * @param isIncognitoIntent Whether the allocated id is for an Incognito window.
     */
    public abstract AllocatedIdInfo allocInstanceId(
            int windowId, int taskId, boolean preferNew, boolean isIncognitoIntent);

    /**
     * Initialize the manager with the allocated instance ID, and perform other post-inflation
     * activity startup tasks, including relevant instance state persistence.
     *
     * @param instanceId Instance ID of the activity.
     * @param taskId Task ID of the activity.
     * @param profileType The type of tab/profile the activity supports.
     */
    public void initialize(int instanceId, int taskId, @SupportedProfileType int profileType) {}

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
     * Open the window for the specified instance. If a live activity exists for the instance, it
     * will be brought to the foreground. If the instance is inactive, it will be restored in a new
     * activity in a new task.
     *
     * @param instanceId ID of the instance whose window should be brought to the foreground.
     * @param source The {@link NewWindowAppSource} that reflects the source of new activity
     *     creation for an inactive instance, used for metrics.
     */
    public void openWindow(int instanceId, @NewWindowAppSource int source) {}

    /**
     * Close the windows associated with a given task / activity. This will permanently and
     * irreversibly delete persisted instances and tab state data.
     *
     * @param instanceIds A list of IDs of the activity instance.
     * @param source The {@link CloseWindowAppSource} that reflects the source of instance closure.
     */
    public void closeWindows(List<Integer> instanceIds, @CloseWindowAppSource int source) {}

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
     * Shows a message to notify the user that a new window cannot be created because {@link
     * MultiWindowUtils#getMaxInstances()} activities already exist.
     */
    public void showInstanceCreationLimitMessage() {
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
