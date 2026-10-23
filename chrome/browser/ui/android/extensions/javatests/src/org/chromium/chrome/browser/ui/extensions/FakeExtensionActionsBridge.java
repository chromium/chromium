// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import org.jni_zero.JniPtr;
import org.jni_zero.JniUniquePtr;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask.NativeBrowserWindowInterface;

import java.util.HashMap;
import java.util.Map;
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
     * A map of native browser windows, as returned by {@link
     * ChromeAndroidTask#getOrCreateNativeBrowserWindow()}, to their corresponding {@link
     * TaskModel}.
     *
     * <p>Keys are compared by identity, which works because {@link
     * ChromeAndroidTask#getOrCreateNativeBrowserWindow()} returns the same instance for a given
     * task and profile.
     */
    private final Map<JniPtr<NativeBrowserWindowInterface>, TaskModel> mTaskModels =
            new HashMap<>();

    /**
     * A map of bridge IDs to their corresponding native browser windows.
     *
     * <p>Bridge IDs is a concept internal to this class, identifying a {@link
     * ExtensionActionsBridge} by a unique long. An increasing non-negative long is assigned to a
     * {@link ExtensionActionsBridge} as it is initialized.
     *
     * <p>Note that we never remove items from this map, which should be fine for tests.
     */
    private final TreeMap<Long, JniPtr<NativeBrowserWindowInterface>> mBridgeIdToBrowserWindow =
            new TreeMap<>();

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
        return mTaskModels.get(getBrowserWindow(task, profile));
    }

    /**
     * Returns the {@link TaskModel} for the given {@link ChromeAndroidTask}, creating it if it
     * doesn't exist.
     */
    public TaskModel getOrCreateTaskModel(ChromeAndroidTask task, Profile profile) {
        JniPtr<NativeBrowserWindowInterface> browserWindow = getBrowserWindow(task, profile);
        TaskModel model = mTaskModels.get(browserWindow);
        if (model == null) {
            model = new TaskModel(task);
            mTaskModels.put(browserWindow, model);
        }
        return model;
    }

    /** Clears all {@link TaskModel} created. */
    public void clear() {
        mTaskModels.clear();
    }

    private static JniPtr<NativeBrowserWindowInterface> getBrowserWindow(
            ChromeAndroidTask task, Profile profile) {
        JniPtr<NativeBrowserWindowInterface> browserWindow =
                task.getOrCreateNativeBrowserWindow(profile);
        assert browserWindow != null
                : "ChromeAndroidTask#getOrCreateNativeBrowserWindow() returned null";
        return browserWindow;
    }

    /**
     * A fake model for a browser window (task), which contains a set of extension actions and other
     * window-specific states.
     */
    public static class TaskModel {
        /** The task associated with this model. */
        private final ChromeAndroidTask mTask;

        private TaskModel(ChromeAndroidTask task) {
            mTask = task;
        }
    }

    /** The JNI bridge implementation. */
    private class FakeExtensionActionsBridgeJni implements ExtensionActionsBridge.Natives {
        private FakeExtensionActionsBridgeJni() {}

        @Override
        public JniUniquePtr<ExtensionActionsBridge.NativeExtensionActionsBridge> init(
                ExtensionActionsBridge bridge, JniPtr<NativeBrowserWindowInterface> browserWindow) {
            // Use the bridge ID as the fake native bridge pointer.
            return JniUniquePtr.createForTesting(allocateBridgeId(browserWindow));
        }

        /** Allocates a new bridge ID for the given native browser window. */
        private long allocateBridgeId(JniPtr<NativeBrowserWindowInterface> browserWindow) {
            long bridgeId = mBridgeIdToBrowserWindow.size() + 1L; // Start bridge IDs at 1.
            mBridgeIdToBrowserWindow.put(bridgeId, browserWindow);
            return bridgeId;
        }
    }
}
