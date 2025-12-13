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
    public @Nullable TaskModel getTaskModel(ChromeAndroidTask task) {
        return mTaskModels.get(computeTaskId(task));
    }

    /**
     * Returns the {@link TaskModel} for the given {@link ChromeAndroidTask}, creating it if it
     * doesn't exist.
     */
    public TaskModel getOrCreateTaskModel(ChromeAndroidTask task) {
        long browserId = computeTaskId(task);
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
    private static long computeTaskId(ChromeAndroidTask task) {
        long taskId = task.getOrCreateNativeBrowserWindowPtr();
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

        /** The bridge for this task. */
        private final ExtensionActionsBridge mBridge;

        /** Whether the model has been initialized. */
        private boolean mInitialized;

        /** The key event handler for this task. */
        private @Nullable KeyEventHandler mKeyEventHandler;

        /** A map of action IDs to functions that return the action data for a given tab ID. */
        private final TreeMap<String, Function<Integer, ActionData>> mActionFuncs = new TreeMap<>();

        private TaskModel(ChromeAndroidTask task) {
            mTask = task;
            // Use the task ID as the native bridge pointer.
            mBridge = new ExtensionActionsBridge(computeTaskId(task));
        }

        public ExtensionActionsBridge getBridge() {
            return mBridge;
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
                getBridge().onActionModelInitialized();
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
                getBridge().onActionUpdated(actionId);
            } else {
                getBridge().onActionAdded(actionId);
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
            getBridge().onActionIconUpdated(actionId);
        }

        /** Removes an extension action. It does nothing if the action ID is not registered. */
        public void removeAction(String actionId) {
            if (!mActionFuncs.containsKey(actionId)) {
                return;
            }
            mActionFuncs.remove(actionId);
            getBridge().onActionRemoved(actionId);
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
        public ExtensionActionsBridge get(long taskId) {
            return getTaskModelOrThrow(taskId).getBridge();
        }

        @Override
        public boolean areActionsInitialized(long nativeExtensionActionsBridge) {
            return getTaskModelOrThrow(nativeExtensionActionsBridge).isInitialized();
        }

        @Override
        public String[] getActionIds(long nativeExtensionActionsBridge) {
            return getTaskModelOrThrow(nativeExtensionActionsBridge)
                    .getIds()
                    .toArray(String[]::new);
        }

        @Override
        public @Nullable ExtensionAction getAction(
                long nativeExtensionActionsBridge, String actionId, int tabId) {
            return getTaskModelOrThrow(nativeExtensionActionsBridge)
                    .getAction(actionId, tabId)
                    .toExtensionAction(actionId);
        }

        @Override
        public @Nullable Bitmap getActionIcon(
                long nativeExtensionActionsBridge,
                String actionId,
                int tabId,
                @Nullable WebContents webContents,
                int canvasWidthDp,
                int canvasHeightDp,
                float scaleFactor) {
            return getTaskModelOrThrow(nativeExtensionActionsBridge)
                    .getAction(actionId, tabId)
                    // The current icon test implementation merely returns a pre-defined icon and
                    // therefore does not need use the canvas dimensions, scale factor, or
                    // webContents for our test cases now.
                    .getIcon();
        }

        @Override
        public @ShowAction int runAction(
                long nativeExtensionActionsBridge,
                String actionId,
                int tabId,
                WebContents webContents) {
            return getTaskModelOrThrow(nativeExtensionActionsBridge)
                    .getAction(actionId, tabId)
                    .getActionRunner()
                    .runAction();
        }

        @Override
        public ExtensionActionsBridge.HandleKeyEventResult handleKeyDownEvent(
                long nativeExtensionActionsBridge, KeyEvent keyEvent) {
            KeyEventHandler keyEventHandler =
                    getTaskModelOrThrow(nativeExtensionActionsBridge).getKeyEventHandler();
            if (keyEventHandler == null) {
                return new ExtensionActionsBridge.HandleKeyEventResult(false, "");
            }
            return keyEventHandler.handleKeyDownEvent(keyEvent);
        }

        /**
         * Returns the {@link TaskModel} for the given task ID, or throws a {@link RuntimeException}
         * if it doesn't exist.
         */
        private TaskModel getTaskModelOrThrow(long taskId) {
            TaskModel model = mTaskModels.get(taskId);
            if (model == null) {
                throw new RuntimeException("TaskModel not created for task " + taskId);
            }
            return model;
        }
    }
}
