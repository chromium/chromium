// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.profiles.Profile;

/** A JNI bridge providing access to information of actions in the toolbar. */
public class ToolbarActionsBridge {
    private long mNativeToolbarActionsBridge;
    @NonNull private final ObserverList<Observer> mObservers = new ObserverList<>();

    @CalledByNative
    private ToolbarActionsBridge(long nativeToolbarActionsBridge) {
        mNativeToolbarActionsBridge = nativeToolbarActionsBridge;
    }

    /** Returns an instance for the given profile. */
    @NonNull
    public static ToolbarActionsBridge get(@NonNull Profile profile) {
        return ToolbarActionsBridgeJni.get().get(profile);
    }

    @CalledByNative
    private void destroy() {
        assert mNativeToolbarActionsBridge != 0;
        mNativeToolbarActionsBridge = 0;
    }

    public void addObserver(@NonNull Observer observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(@NonNull Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Returns whether actions have been initialized.
     *
     * <p>If it returns false, you can install an observer to wait for initialization.
     */
    public boolean areActionsInitialized() {
        return ToolbarActionsBridgeJni.get().areActionsInitialized(mNativeToolbarActionsBridge);
    }

    /** Returns a sorted list of enabled action IDs. */
    @NonNull
    public String[] getActionIds() {
        return ToolbarActionsBridgeJni.get().getActionIds(mNativeToolbarActionsBridge);
    }

    /** Returns the state of an action for a particular tab. */
    @Nullable
    public ToolbarAction getAction(@NonNull String actionId, int tabId) {
        return ToolbarActionsBridgeJni.get()
                .getAction(mNativeToolbarActionsBridge, actionId, tabId);
    }

    @CalledByNative
    private void onToolbarActionAdded(@JniType("std::string") String actionId) {
        for (Observer observer : mObservers) {
            observer.onToolbarActionAdded(actionId);
        }
    }

    @CalledByNative
    private void onToolbarActionRemoved(@JniType("std::string") String actionId) {
        for (Observer observer : mObservers) {
            observer.onToolbarActionRemoved(actionId);
        }
    }

    @CalledByNative
    private void onToolbarActionUpdated(@JniType("std::string") String actionId) {
        for (Observer observer : mObservers) {
            observer.onToolbarActionUpdated(actionId);
        }
    }

    @CalledByNative
    private void onToolbarModelInitialized() {
        for (Observer observer : mObservers) {
            observer.onToolbarModelInitialized();
        }
    }

    @CalledByNative
    private void onToolbarPinnedActionsChanged() {
        for (Observer observer : mObservers) {
            observer.onToolbarPinnedActionsChanged();
        }
    }

    /** The interface for observing action events. */
    public interface Observer {
        /**
         * Signals that actionId has been added to the toolbar. This will only be called after the
         * toolbar model has been initialized.
         */
        void onToolbarActionAdded(@NonNull String actionId);

        /** Signals that the given action with actionId has been removed from the toolbar. */
        void onToolbarActionRemoved(@NonNull String actionId);

        /**
         * Signals that the browser action with actionId has been updated. This method covers lots
         * of different extension updates.
         */
        void onToolbarActionUpdated(@NonNull String actionId);

        /**
         * Signals that the toolbar model has been initialized, so that if any observers were
         * postponing animation during the initialization stage, they can catch up.
         */
        void onToolbarModelInitialized();

        /** Called whenever the pinned actions change. */
        void onToolbarPinnedActionsChanged();
    }

    @NativeMethods
    public interface Natives {
        ToolbarActionsBridge get(@JniType("Profile*") Profile profile);

        boolean areActionsInitialized(long nativeToolbarActionsBridge);

        @JniType("std::vector<std::string>")
        String[] getActionIds(long nativeToolbarActionsBridge);

        ToolbarAction getAction(
                long nativeToolbarActionsBridge,
                @JniType("std::string") String actionId,
                int tabId);
    }
}
