// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.view.KeyEvent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;

import java.util.TreeMap;

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

    /**
     * A fake model for a browser window (task), which contains a set of extension actions and other
     * window-specific states.
     */
    public static class TaskModel {
        /** The task associated with this model. */
        private final ChromeAndroidTask mTask;

        /** The key event handler for this task. */
        private @Nullable KeyEventHandler mKeyEventHandler;

        private TaskModel(ChromeAndroidTask task) {
            mTask = task;
        }

        /** Returns the {@link KeyEventHandler} for this task. */
        public @Nullable KeyEventHandler getKeyEventHandler() {
            return mKeyEventHandler;
        }

        /** Sets the {@link KeyEventHandler} for this task. */
        public void setKeyEventHandler(@Nullable KeyEventHandler keyEventHandler) {
            mKeyEventHandler = keyEventHandler;
        }
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
            // Use the bridge ID as the native bridge pointer.
            return allocateBridgeId(taskId);
        }

        @Override
        public void destroy(long bridgeId) {}

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
