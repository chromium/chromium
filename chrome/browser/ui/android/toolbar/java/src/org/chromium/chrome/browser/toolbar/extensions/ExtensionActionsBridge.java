// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;
import org.chromium.extensions.ShowAction;

/** A JNI bridge providing access to information of extension actions in the toolbar. */
@NullMarked
public class ExtensionActionsBridge {
    private long mNativeExtensionActionsBridge;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    @CalledByNative
    @VisibleForTesting
    ExtensionActionsBridge(long nativeExtensionActionsBridge) {
        mNativeExtensionActionsBridge = nativeExtensionActionsBridge;
    }

    /** Returns an instance for the given profile. */
    public static ExtensionActionsBridge get(Profile profile) {
        return ExtensionActionsBridgeJni.get().get(profile);
    }

    @CalledByNative
    private void destroy() {
        assert mNativeExtensionActionsBridge != 0;
        mNativeExtensionActionsBridge = 0;
    }

    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Returns whether actions have been initialized.
     *
     * <p>If it returns false, you can install an observer to wait for initialization.
     */
    public boolean areActionsInitialized() {
        return ExtensionActionsBridgeJni.get().areActionsInitialized(mNativeExtensionActionsBridge);
    }

    /** Returns a sorted list of enabled action IDs. */
    public String[] getActionIds() {
        return ExtensionActionsBridgeJni.get().getActionIds(mNativeExtensionActionsBridge);
    }

    /** Returns the state of an action for a particular tab. */
    @Nullable
    public ExtensionAction getAction(String actionId, int tabId) {
        return ExtensionActionsBridgeJni.get()
                .getAction(mNativeExtensionActionsBridge, actionId, tabId);
    }

    /**
     * Returns the icon for the action in the specified tab.
     *
     * <p>While loading the icon, this method returns a transparent icon.
     */
    @Nullable
    public Bitmap getActionIcon(String actionId, int tabId) {
        return ExtensionActionsBridgeJni.get()
                .getActionIcon(mNativeExtensionActionsBridge, actionId, tabId);
    }

    /**
     * Runs an extension action.
     *
     * <p>It returns a {@link ShowAction} enum indicating what UI action the caller should perform.
     */
    public @ShowAction int runAction(String actionId, WebContents webContents) {
        return ExtensionActionsBridgeJni.get()
                .runAction(mNativeExtensionActionsBridge, actionId, webContents);
    }

    @CalledByNative
    @VisibleForTesting
    void onActionAdded(@JniType("std::string") String actionId) {
        for (Observer observer : mObservers) {
            observer.onActionAdded(actionId);
        }
    }

    @CalledByNative
    @VisibleForTesting
    void onActionRemoved(@JniType("std::string") String actionId) {
        for (Observer observer : mObservers) {
            observer.onActionRemoved(actionId);
        }
    }

    @CalledByNative
    @VisibleForTesting
    void onActionUpdated(@JniType("std::string") String actionId) {
        for (Observer observer : mObservers) {
            observer.onActionUpdated(actionId);
        }
    }

    @CalledByNative
    @VisibleForTesting
    void onActionModelInitialized() {
        for (Observer observer : mObservers) {
            observer.onActionModelInitialized();
        }
    }

    @CalledByNative
    @VisibleForTesting
    void onPinnedActionsChanged() {
        for (Observer observer : mObservers) {
            observer.onPinnedActionsChanged();
        }
    }

    @CalledByNative
    @VisibleForTesting
    void onActionIconUpdated(@JniType("std::string") String actionId) {
        for (Observer observer : mObservers) {
            observer.onActionIconUpdated(actionId);
        }
    }

    /** The interface for observing action events. */
    public interface Observer {
        /**
         * Signals that actionId has been added to the toolbar. This will only be called after the
         * toolbar model has been initialized.
         */
        void onActionAdded(String actionId);

        /** Signals that the given action with actionId has been removed from the toolbar. */
        void onActionRemoved(String actionId);

        /**
         * Signals that the browser action with actionId has been updated. This method covers lots
         * of different extension updates, except for icons which should be covered by {@link
         * #onActionIconUpdated()}.
         */
        void onActionUpdated(String actionId);

        /**
         * Signals that the toolbar model has been initialized, so that if any observers were
         * postponing animation during the initialization stage, they can catch up.
         */
        void onActionModelInitialized();

        /** Called whenever the pinned actions change. */
        void onPinnedActionsChanged();

        /** Called when the icon for an action was updated. */
        void onActionIconUpdated(String actionId);
    }

    @NativeMethods
    public interface Natives {
        ExtensionActionsBridge get(@JniType("Profile*") Profile profile);

        boolean areActionsInitialized(long nativeExtensionActionsBridge);

        @JniType("std::vector<std::string>")
        String[] getActionIds(long nativeExtensionActionsBridge);

        @Nullable ExtensionAction getAction(
                long nativeExtensionActionsBridge,
                @JniType("std::string") String actionId,
                int tabId);

        @Nullable Bitmap getActionIcon(
                long nativeExtensionActionsBridge,
                @JniType("std::string") String actionId,
                int tabId);

        int runAction(
                long nativeExtensionActionsBridge,
                @JniType("std::string") String actionId,
                WebContents webContents);
    }
}
