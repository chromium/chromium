// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.KeyEvent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.content_public.browser.WebContents;
import org.chromium.extensions.ShowAction;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Objects;
import java.util.TreeMap;
import java.util.function.Function;

/**
 * A fake {@link ExtensionActionsBridge} for use in tests.
 *
 * <p>This class should be used to model the behavior of the C++ extension backend the Java UI
 * without needing to build all of the native components.
 */
@NullMarked
public class FakeExtensionActionsBridge {
    /**
     * A map of task IDs to their corresponding {@link TaskModel}.
     *
     * <p>Task IDs is a concept internal to this class, identifying a {@link ChromeAndroidTask} that
     * is associated with {@link TaskModel}. It is defined to be identical to the native browser
     * window interface pointer as returned by {@link
     * ChromeAndroidTask#getOrCreateNativeBrowserWindowPtr()}. We ensure that it is non-zero to
     * avoid potential bugs with mocked {@link ChromeAndroidTask}.
     */
    private final TreeMap<Long, TaskModel> mTaskModels = new TreeMap<>();

    /**
     * A map of bridge IDs to their corresponding task IDs.
     *
     * <p>Bridge IDs is a concept internal to this class, identifying a {@link
     * ExtensionActionsBridge} by a unique long. An increasing non-negative long is assigned to a
     * {@link ExtensionActionsBridge} as it is initialized.
     *
     * <p>Note that we never remove items from this map, which should be fine for tests.
     */
    private final TreeMap<Long, Long> mBridgeIdToTaskId = new TreeMap<>();

    public FakeExtensionActionsBridge() {}

    /**
     * Installs this fake bridge. This should be called before any UI component that uses {@link
     * ExtensionActionsBridge} is created.
     */
    public void install() {
        ExtensionActionsBridgeJni.setInstanceForTesting(new FakeExtensionActionsBridgeJni());
    }

    /** Uninstalls this fake bridge. */
    public void uninstall() {
        ExtensionActionsBridgeJni.setInstanceForTesting(null);
    }

    /**
     * Returns the {@link TaskModel} for the given {@link ChromeAndroidTask}, or null if it doesn't
     * exist.
     */
    public @Nullable TaskModel getTaskModel(ChromeAndroidTask task, Profile profile) {
        return mTaskModels.get(computeTaskId(task, profile));
    }

    /**
     * Returns the {@link TaskModel} for the given {@link ChromeAndroidTask}, creating it if it
     * doesn't exist.
     */
    public TaskModel getOrCreateTaskModel(ChromeAndroidTask task, Profile profile) {
        long browserId = computeTaskId(task, profile);
        TaskModel model = mTaskModels.get(browserId);
        if (model == null) {
            model = new TaskModel(task);
            mTaskModels.put(browserId, model);
        }
        return model;
    }

    /** Clears all {@link TaskModel} created. */
    public void clear() {
        mTaskModels.clear();
    }

    /** Computes the task ID for the given {@link ChromeAndroidTask}. */
    private static long computeTaskId(ChromeAndroidTask task, Profile profile) {
        long taskId = task.getOrCreateNativeBrowserWindowPtr(profile);
        assert taskId != 0 : "ChromeAndroidTask#getOrCreateNativeBrowserWindowPtr() returned 0";
        return taskId;
    }

    /** Creates a new transparent icon. */
    private static Bitmap createTransparentIcon() {
        Bitmap bitmap = Bitmap.createBitmap(12, 12, Bitmap.Config.ARGB_8888);
        bitmap.eraseColor(Color.TRANSPARENT);
        return bitmap;
    }

    /**
     * A fake model for a browser window (task), which contains a set of extension actions and other
     * window-specific states.
     */
    public static class TaskModel {
        /** The task associated with this model. */
        private final ChromeAndroidTask mTask;

        /** The active bridges for this task, keyed by their bridge IDs. */
        private final TreeMap<Long, ExtensionActionsBridge> mActiveBridges = new TreeMap<>();

        /** Whether the model has been initialized. */
        private boolean mInitialized;

        /** The key event handler for this task. */
        private @Nullable KeyEventHandler mKeyEventHandler;

        /** A map of action IDs to functions that return the action data for a given tab ID. */
        private final TreeMap<String, Function<Integer, ActionData>> mActionFuncs = new TreeMap<>();

        private TaskModel(ChromeAndroidTask task) {
            mTask = task;
        }

        private Collection<ExtensionActionsBridge> getActiveBridges() {
            return List.copyOf(mActiveBridges.values());
        }

        /** Returns whether the model has been initialized. */
        public boolean isInitialized() {
            return mInitialized;
        }

        /**
         * Sets whether the model is initialized. Default is false.
         *
         * <p>Calling this method with true will notify all observers that the model is ready.
         * Beware that the notification is sent only once in production.
         */
        public void setInitialized(boolean initialized) {
            mInitialized = initialized;
            if (mInitialized) {
                for (ExtensionActionsBridge bridge : getActiveBridges()) {
                    bridge.onActionModelInitialized();
                }
            }
        }

        /** Returns the {@link KeyEventHandler} for this task. */
        public @Nullable KeyEventHandler getKeyEventHandler() {
            return mKeyEventHandler;
        }

        /** Sets the {@link KeyEventHandler} for this task. */
        public void setKeyEventHandler(@Nullable KeyEventHandler keyEventHandler) {
            mKeyEventHandler = keyEventHandler;
        }

        /** Returns the {@link ActionData} for the given actionId and tabId. */
        public ActionData getAction(String actionId, int tabId) {
            Function<Integer, ActionData> actionFunc = mActionFuncs.get(actionId);
            assert actionFunc != null;
            return actionFunc.apply(tabId);
        }

        /**
         * Adds or updates an extension action. If the action already exists, it will be updated.
         */
        public void putAction(String actionId, ActionData action) {
            putAction(actionId, (tabId) -> action);
        }

        /**
         * Adds or updates an extension action. If the action already exists, it will be updated.
         */
        public void putAction(String actionId, Function<Integer, ActionData> actionFunc) {
            boolean update = mActionFuncs.containsKey(actionId);
            mActionFuncs.put(actionId, actionFunc);

            if (update) {
                for (ExtensionActionsBridge bridge : getActiveBridges()) {
                    bridge.onActionUpdated(actionId);
                }
            } else {
                for (ExtensionActionsBridge bridge : getActiveBridges()) {
                    bridge.onActionAdded(actionId);
                }
            }
        }

        /**
         * Updates the icon for an extension action.
         *
         * <p>The action ID should be already added to the model. While you can update any field of
         * {@link ActionData} with this method, observers are notified only about icon updates.
         */
        public void updateActionIcon(String actionId, ActionData action) {
            updateActionIcon(actionId, (tabId) -> action);
        }

        /**
         * Updates the icon for an extension action.
         *
         * <p>The action ID should be already added to the model. While you can update any field of
         * {@link ActionData} with this method, observers are notified only about icon updates.
         */
        public void updateActionIcon(String actionId, Function<Integer, ActionData> actionFunc) {
            assert mActionFuncs.containsKey(actionId);

            mActionFuncs.put(actionId, actionFunc);
            for (ExtensionActionsBridge bridge : getActiveBridges()) {
                bridge.onActionIconUpdated(actionId);
            }
        }

        /** Removes an extension action. It does nothing if the action ID is not registered. */
        public void removeAction(String actionId) {
            if (!mActionFuncs.containsKey(actionId)) {
                return;
            }
            mActionFuncs.remove(actionId);
            for (ExtensionActionsBridge bridge : getActiveBridges()) {
                bridge.onActionRemoved(actionId);
            }
        }

        /** Returns the list of all action IDs, sorted lexicographically. */
        public List<String> getIds() {
            return new ArrayList<>(mActionFuncs.keySet());
        }
    }

    /** An immutable representation of an extension action. */
    public static class ActionData {
        private final String mTitle;
        private final Bitmap mIcon;
        private final ActionRunner mActionRunner;

        private ActionData(String title, Bitmap icon, ActionRunner actionRunner) {
            mTitle = title;
            mIcon = icon;
            mActionRunner = actionRunner;
        }

        public String getTitle() {
            return mTitle;
        }

        public Bitmap getIcon() {
            return mIcon;
        }

        public ActionRunner getActionRunner() {
            return mActionRunner;
        }

        public ExtensionAction toExtensionAction(String actionId) {
            return new ExtensionAction(actionId, mTitle);
        }

        public Builder toBuilder() {
            return new Builder().setTitle(mTitle).setIcon(mIcon).setActionRunner(mActionRunner);
        }

        public static class Builder {
            private String mTitle = "";
            private Bitmap mIcon = createTransparentIcon();
            private ActionRunner mActionRunner = () -> ShowAction.NONE;

            public Builder() {}

            public Builder setTitle(String title) {
                mTitle = title;
                return this;
            }

            public Builder setIcon(Bitmap icon) {
                mIcon =
                        icon.copy(
                                Objects.requireNonNull(
                                        icon.getConfig(), "Test icons must have a valid config"),
                                /* isMutable= */ false);
                return this;
            }

            public Builder setActionRunner(ActionRunner actionRunner) {
                mActionRunner = actionRunner;
                return this;
            }

            public ActionData build() {
                return new ActionData(mTitle, mIcon, mActionRunner);
            }
        }
    }

    /** An interface for an extension action. */
    public interface ActionRunner {
        /** Runs the fake action. */
        @ShowAction
        int runAction();
    }

    /** An interface for handling key events. */
    public interface KeyEventHandler {
        /** Handles a key down event. */
        ExtensionActionsBridge.HandleKeyEventResult handleKeyDownEvent(KeyEvent keyEvent);
    }

    /** The JNI bridge implementation. */
    private class FakeExtensionActionsBridgeJni implements ExtensionActionsBridge.Natives {
        private FakeExtensionActionsBridgeJni() {}

        @Override
        public boolean extensionsEnabled(Profile profile) {
            return true;
        }

        @Override
        public long init(ExtensionActionsBridge bridge, long taskId) {
            long bridgeId = allocateBridgeId(taskId);
            TaskModel taskModel = getTaskModelByTaskIdOrThrow(taskId);
            taskModel.mActiveBridges.put(bridgeId, bridge);
            // Use the bridge ID as the native bridge pointer.
            return bridgeId;
        }

        @Override
        public void destroy(long bridgeId) {
            getTaskModelByBridgeIdOrThrow(bridgeId).mActiveBridges.remove(bridgeId);
        }

        @Override
        public boolean areActionsInitialized(long bridgeId) {
            return getTaskModelByBridgeIdOrThrow(bridgeId).isInitialized();
        }

        @Override
        public String[] getActionIds(long bridgeId) {
            return getTaskModelByBridgeIdOrThrow(bridgeId).getIds().toArray(String[]::new);
        }

        @Override
        public @Nullable ExtensionAction getAction(long bridgeId, String actionId, int tabId) {
            return getTaskModelByBridgeIdOrThrow(bridgeId)
                    .getAction(actionId, tabId)
                    .toExtensionAction(actionId);
        }

        @Override
        public @Nullable Bitmap getActionIcon(
                long bridgeId,
                String actionId,
                int tabId,
                @Nullable WebContents webContents,
                int canvasWidthDp,
                int canvasHeightDp,
                float scaleFactor) {
            return getTaskModelByBridgeIdOrThrow(bridgeId)
                    .getAction(actionId, tabId)
                    // The current icon test implementation merely returns a pre-defined icon and
                    // therefore does not need use the canvas dimensions, scale factor, or
                    // webContents for our test cases now.
                    .getIcon();
        }

        @Override
        public @ShowAction int runAction(
                long bridgeId, String actionId, int tabId, WebContents webContents) {
            return getTaskModelByBridgeIdOrThrow(bridgeId)
                    .getAction(actionId, tabId)
                    .getActionRunner()
                    .runAction();
        }

        @Override
        public ExtensionActionsBridge.HandleKeyEventResult handleKeyDownEvent(
                long bridgeId, KeyEvent keyEvent) {
            KeyEventHandler keyEventHandler =
                    getTaskModelByBridgeIdOrThrow(bridgeId).getKeyEventHandler();
            if (keyEventHandler == null) {
                return new ExtensionActionsBridge.HandleKeyEventResult(false, "");
            }
            return keyEventHandler.handleKeyDownEvent(keyEvent);
        }

        /**
         * Returns the {@link TaskModel} for the given task ID.
         *
         * @throws RuntimeException if the task ID is not known.
         */
        private TaskModel getTaskModelByTaskIdOrThrow(long taskId) {
            TaskModel model = mTaskModels.get(taskId);
            if (model == null) {
                throw new RuntimeException("TaskModel not created for task " + taskId);
            }
            return model;
        }

        /** Allocates a new bridge ID for the given task ID. */
        private long allocateBridgeId(long taskId) {
            long bridgeId = mBridgeIdToTaskId.size() + 1L; // Start bridge IDs at 1.
            mBridgeIdToTaskId.put(bridgeId, taskId);
            return bridgeId;
        }

        /**
         * Returns the {@link TaskModel} for the given bridge ID.
         *
         * @throws RuntimeException if the bridge ID is not known.
         */
        private TaskModel getTaskModelByBridgeIdOrThrow(long bridgeId) {
            Long taskId = mBridgeIdToTaskId.get(bridgeId);
            if (taskId == null) {
                throw new RuntimeException("Bridge " + bridgeId + " not known");
            }
            return getTaskModelByTaskIdOrThrow(taskId);
        }
    }
}
