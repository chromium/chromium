// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.toolbar.InvocationSource;
import org.chromium.content_public.browser.WebContents;

/** A JNI bridge to interact with extension actions for the toolbar. */
@NullMarked
@JNINamespace("extensions")
public class ExtensionsToolbarBridge implements Destroyable {
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private long mNativeExtensionsToolbarBridge;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    // The delegate is set via a setter because of a bidirectional dependency
    // with {@code ExtensionActionListMediator}.
    private @Nullable Delegate mDelegate;

    public ExtensionsToolbarBridge(ChromeAndroidTask task) {
        mNativeExtensionsToolbarBridge =
                ExtensionsToolbarBridgeJni.get()
                        .init(this, task.getOrCreateNativeBrowserWindowPtr());
    }

    @Override
    public void destroy() {
        assert mNativeExtensionsToolbarBridge != 0;
        ExtensionsToolbarBridgeJni.get().destroy(mNativeExtensionsToolbarBridge);
        mNativeExtensionsToolbarBridge = 0;
        LifetimeAssert.destroy(mLifetimeAssert);
    }

    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    public void setDelegate(@Nullable Delegate delegate) {
        mDelegate = delegate;
    }

    @Nullable
    public ExtensionAction getAction(String actionId) {
        return ExtensionsToolbarBridgeJni.get().getAction(mNativeExtensionsToolbarBridge, actionId);
    }

    @Nullable
    public Bitmap getIcon(
            String actionId,
            @Nullable WebContents webContents,
            int canvasWidthDp,
            int canvasHeightDp,
            float scaleFactor) {
        return ExtensionsToolbarBridgeJni.get()
                .getIcon(
                        mNativeExtensionsToolbarBridge,
                        actionId,
                        webContents,
                        canvasWidthDp,
                        canvasHeightDp,
                        scaleFactor);
    }

    public String[] getAllActionIds() {
        return ExtensionsToolbarBridgeJni.get().getAllActionIds(mNativeExtensionsToolbarBridge);
    }

    public String[] getPinnedActionIds() {
        return ExtensionsToolbarBridgeJni.get().getPinnedActionIds(mNativeExtensionsToolbarBridge);
    }

    public void executeUserAction(String actionId, @InvocationSource int source) {
        ExtensionsToolbarBridgeJni.get()
                .executeUserAction(mNativeExtensionsToolbarBridge, actionId, source);
    }

    @CalledByNative
    public void triggerPopup(@JniType("std::string") String actionId, long nativeHostPtr) {
        // {@link mDelegate} should be set in {@code ExtensionActionListMediator}'s constructor.
        assert mDelegate != null;

        mDelegate.triggerPopup(actionId, nativeHostPtr);
    }

    @CalledByNative
    public void onActionsInitialized() {
        for (Observer observer : mObservers) {
            observer.onActionsInitialized();
        }
    }

    @CalledByNative
    public void onActionAdded(@JniType("std::string") String actionId) {
        for (Observer observer : mObservers) {
            observer.onActionAdded(actionId);
        }
    }

    @CalledByNative
    public void onActionRemoved(@JniType("std::string") String actionId) {
        for (Observer observer : mObservers) {
            observer.onActionRemoved(actionId);
        }
    }

    @CalledByNative
    public void onActionUpdated(@JniType("std::string") String actionId) {
        for (Observer observer : mObservers) {
            observer.onActionUpdated(actionId);
        }
    }

    @CalledByNative
    public void onPinnedActionsChanged() {
        for (Observer observer : mObservers) {
            observer.onPinnedActionsChanged();
        }
    }

    @CalledByNative
    public void onActiveWebContentsChanged() {
        for (Observer observer : mObservers) {
            observer.onActiveWebContentsChanged();
        }
    }

    public interface Observer {
        // Called after all actions are added to the model.
        void onActionsInitialized();

        // Called when an action is added to the model.
        void onActionAdded(String actionId);

        // Called when an action is removed from the model.
        void onActionRemoved(String actionId);

        // Called when an action in the model is updated.
        void onActionUpdated(String actionId);

        // Called when the pinned actions in the model are changed.
        void onPinnedActionsChanged();

        // Called when the active web contents changes due to e.g. navigation or tab change.
        void onActiveWebContentsChanged();
    }

    public interface Delegate {
        // Called when the popup should be shown.
        void triggerPopup(String actionId, long nativeHostPtr);
    }

    @NativeMethods
    public interface Natives {
        long init(ExtensionsToolbarBridge bridge, long browserWindowInterfacePtr);

        void destroy(long nativeExtensionsToolbarBridge);

        @Nullable ExtensionAction getAction(
                long nativeExtensionsToolbarBridge, @JniType("std::string") String actionId);

        @Nullable Bitmap getIcon(
                long nativeExtensionsToolbarBridge,
                @JniType("std::string") String actionId,
                @Nullable @JniType("content::WebContents*") WebContents webContents,
                int canvasWidthDp,
                int canvasHeightDp,
                float scaleFactor);

        @JniType("std::vector<std::string>")
        String[] getAllActionIds(long nativeExtensionsToolbarBridge);

        @JniType("std::vector<std::string>")
        String[] getPinnedActionIds(long nativeExtensionsToolbarBridge);

        void executeUserAction(
                long nativeExtensionsToolbarBridge,
                @JniType("std::string") String actionId,
                @JniType("ToolbarActionViewModel::InvocationSource") int source);
    }
}
