// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.graphics.Bitmap;
import android.view.KeyEvent;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;
import org.chromium.extensions.ShowAction;

import java.util.Objects;

/** A JNI bridge to interact with extension actions for the toolbar. */
@NullMarked
@JNINamespace("extensions")
public class ExtensionActionsBridge {
    private long mNativeExtensionActionsBridge;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    @CalledByNative
    @VisibleForTesting
    public ExtensionActionsBridge(long nativeExtensionActionsBridge) {
        mNativeExtensionActionsBridge = nativeExtensionActionsBridge;
    }

    /** Returns an instance for the given profile. */
    public static ExtensionActionsBridge get(Profile profile) {
        return ExtensionActionsBridgeJni.get().get(profile);
    }

    /** Represents the result of handling a key event. */
    public static class HandleKeyEventResult {
        /** Whether the key event has been handled in C++. */
        public final boolean handled;

        /** The action to trigger, or empty if no action should be triggered. */
        public final String actionId;

        @VisibleForTesting
        @CalledByNative("HandleKeyEventResult")
        public HandleKeyEventResult(boolean handled, @JniType("std::string") String actionId) {
            this.handled = handled;
            this.actionId = actionId;
        }

        @Override
        public boolean equals(Object o) {
            if (o instanceof HandleKeyEventResult other) {
                return handled == other.handled && actionId.equals(other.actionId);
            }
            return false;
        }

        @Override
        public int hashCode() {
            return Objects.hash(handled, actionId);
        }
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
    public Bitmap getActionIcon(
            String actionId,
            int tabId,
            @Nullable WebContents webContents,
            int canvasWidthDp,
            int canvasHeightDp,
            float scaleFactor) {
        return ExtensionActionsBridgeJni.get()
                .getActionIcon(
                        mNativeExtensionActionsBridge,
                        actionId,
                        tabId,
                        webContents,
                        canvasWidthDp,
                        canvasHeightDp,
                        scaleFactor);
    }

    /**
     * Runs an extension action.
     *
     * <p>It returns a {@link ShowAction} enum indicating what UI action the caller should perform.
     */
    public @ShowAction int runAction(String actionId, int tabId, WebContents webContents) {
        // NOTE: The underlying JNI implementation does not use the tab ID because it can extract it
        // from WebContents with SessionTabHelper::IdForTab(). However, doing similar is difficult
        // in tests for two reasons: (1) there is no existing JNI method to call into IdForTab();
        // (2) even if we introduce such a JNI method, the passed WebContents is often a mock and
        // doesn't have an associated SessionTabHelper, so IdForTab() would not work. Fortunately it
        // should be easy for callers of this method to find the tab ID, so we ask them to pass it
        // as an argument, even though the value is not used in production.
        return ExtensionActionsBridgeJni.get()
                .runAction(mNativeExtensionActionsBridge, actionId, tabId, webContents);
    }

    /**
     * Returns whether the extensions are disabled on the profile for Desktop Android. This is
     * temporary for until extensions are ready for dogfooding. TODO(crbug.com/422307625): Remove
     * this check once extensions are ready for dogfooding.
     */
    public static boolean extensionsEnabled(Profile profile) {
        return ExtensionActionsBridgeJni.get().extensionsEnabled(profile);
    }

    /** Handles the key down event and returns the result. */
    public HandleKeyEventResult handleKeyDownEvent(KeyEvent event) {
        return ExtensionActionsBridgeJni.get()
                .handleKeyDownEvent(mNativeExtensionActionsBridge, event);
    }

    @CalledByNative
    @VisibleForTesting
    public void onActionAdded(@JniType("std::string") String actionId) {
        for (Observer observer : mObservers) {
            observer.onActionAdded(actionId);
        }
    }

    @CalledByNative
    @VisibleForTesting
    public void onActionRemoved(@JniType("std::string") String actionId) {
        for (Observer observer : mObservers) {
            observer.onActionRemoved(actionId);
        }
    }

    @CalledByNative
    @VisibleForTesting
    public void onActionUpdated(@JniType("std::string") String actionId) {
        for (Observer observer : mObservers) {
            observer.onActionUpdated(actionId);
        }
    }

    @CalledByNative
    @VisibleForTesting
    public void onActionModelInitialized() {
        for (Observer observer : mObservers) {
            observer.onActionModelInitialized();
        }
    }

    @CalledByNative
    @VisibleForTesting
    public void onPinnedActionsChanged() {
        for (Observer observer : mObservers) {
            observer.onPinnedActionsChanged();
        }
    }

    @CalledByNative
    @VisibleForTesting
    public void onActionIconUpdated(@JniType("std::string") String actionId) {
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
        boolean extensionsEnabled(@JniType("Profile*") Profile profile);

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
                int tabId,
                @Nullable @JniType("content::WebContents*") WebContents webContents,
                int canvasWidthDp,
                int canvasHeightDp,
                float scaleFactor);

        @JniType("ExtensionAction::ShowAction")
        int runAction(
                long nativeExtensionActionsBridge,
                @JniType("std::string") String actionId,
                int tabId,
                @JniType("content::WebContents*") WebContents webContents);

        HandleKeyEventResult handleKeyDownEvent(
                long nativeExtensionActionsBridge,
                @JniType("ui::KeyEventAndroid") KeyEvent keyEvent);
    }
}
