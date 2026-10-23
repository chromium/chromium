// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.graphics.Bitmap;
import android.view.KeyEvent;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask.NativeBrowserWindowInterface;
import org.chromium.chrome.browser.ui.toolbar.InvocationSource;
import org.chromium.content_public.browser.WebContents;

/** A JNI bridge to interact with extension actions for the toolbar. */
@NullMarked
@JNINamespace("extensions")
public class ExtensionsToolbarBridge implements Destroyable {
    @JniType("::extensions::ExtensionsToolbarAndroid")
    interface NativeExtensionsToolbarAndroid extends JniTypeToken {}

    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private final JniUniquePtr<NativeExtensionsToolbarAndroid> mNative;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    // The delegates are set via setters because of bidirectional dependencies.
    private @Nullable ActionListDelegate mActionListDelegate;
    private @Nullable MenuDelegate mMenuDelegate;

    public ExtensionsToolbarBridge(ChromeAndroidTask task, Profile profile) {
        mNative =
                ExtensionsToolbarBridgeJni.get()
                        .init(this, task.getOrCreateNativeBrowserWindow(profile));
    }

    @Override
    public void destroy() {
        mNative.destroy();
        LifetimeAssert.destroy(mLifetimeAssert);
    }

    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    public void setActionListDelegate(@Nullable ActionListDelegate delegate) {
        mActionListDelegate = delegate;
    }

    public void setMenuDelegate(@Nullable MenuDelegate delegate) {
        mMenuDelegate = delegate;
    }

    JniPtr<NativeExtensionsToolbarAndroid> getNativePtr() {
        return mNative;
    }

    @Nullable
    public ExtensionAction getAction(String actionId, @Nullable WebContents webContents) {
        return ExtensionsToolbarBridgeJni.get().getAction(mNative, actionId, webContents);
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
                        mNative, actionId, webContents, canvasWidthDp, canvasHeightDp, scaleFactor);
    }

    public String[] getAllActionIds() {
        return ExtensionsToolbarBridgeJni.get().getAllActionIds(mNative);
    }

    public String[] getPinnedActionIds() {
        return ExtensionsToolbarBridgeJni.get().getPinnedActionIds(mNative);
    }

    public boolean isActionDraggable(String actionId) {
        return ExtensionsToolbarBridgeJni.get().isActionDraggable(mNative, actionId);
    }

    public void executeUserAction(String actionId, @InvocationSource int source) {
        ExtensionsToolbarBridgeJni.get().executeUserAction(mNative, actionId, source);
    }

    public void movePinnedAction(String actionId, int targetIndex) {
        ExtensionsToolbarBridgeJni.get().movePinnedAction(mNative, actionId, targetIndex);
    }

    public boolean onRequestAccessButtonClicked(WebContents webContents) {
        return ExtensionsToolbarBridgeJni.get().onRequestAccessButtonClicked(mNative, webContents);
    }

    public RequestAccessButtonParams getRequestAccessButtonParams(WebContents webContents) {
        RequestAccessButtonParams params =
                ExtensionsToolbarBridgeJni.get().getRequestAccessButtonParams(mNative, webContents);
        assert params != null;
        return params;
    }

    public ExtensionsMenuButtonState getMenuButtonState(
            WebContents webContents,
            int canvasWidthDp,
            int canvasHeightDp,
            float scaleFactor,
            int color) {
        return ExtensionsToolbarBridgeJni.get()
                .getMenuButtonState(
                        mNative, webContents, canvasWidthDp, canvasHeightDp, scaleFactor, color);
    }

    /** Handles the key down event and returns the result. */
    public boolean handleKeyDownEvent(KeyEvent event) {
        return ExtensionsToolbarBridgeJni.get().handleKeyDownEvent(mNative, event);
    }

    @CalledByNative
    public void showManageExtensionsIPH() {
        for (Observer observer : mObservers) {
            observer.showManageExtensionsIPH();
        }
    }

    @CalledByNative
    public void showPinnedByDefaultIPH(@JniType("std::string") String extensionId) {
        for (Observer observer : mObservers) {
            observer.showPinnedByDefaultIPH(extensionId);
        }
    }

    @CalledByNative
    public void triggerPopup(
            @JniType("std::string") String actionId,
            ExtensionActionPopupContents popupContents,
            boolean inspectWithDevTools) {
        // {@link mActionListDelegate} should be set in {@code ExtensionActionListMediator}'s
        // constructor.
        assert mActionListDelegate != null;

        mActionListDelegate.triggerPopup(actionId, popupContents, inspectWithDevTools);
    }

    @CalledByNative
    void showContextMenu(@JniType("std::string") String actionId) {
        // {@link mActionListDelegate} should be set in {@code ExtensionActionListMediator}'s
        // constructor.
        assert mActionListDelegate != null;

        mActionListDelegate.showContextMenu(actionId);
    }

    @CalledByNative
    public void onRequestAccessButtonParamsChanged() {
        for (Observer observer : mObservers) {
            observer.onRequestAccessButtonParamsChanged();
        }
    }

    @CalledByNative
    public void onToolbarControlStateUpdated() {
        for (Observer observer : mObservers) {
            observer.onToolbarControlStateUpdated();
        }
    }

    @CalledByNative
    public boolean hasPoppedOutAction() {
        // {@link mActionListDelegate} should be set in {@code ExtensionActionListMediator}'s
        // constructor.
        assert mActionListDelegate != null;

        return mActionListDelegate.hasPoppedOutAction();
    }

    @CalledByNative
    public void hideActivePopup() {
        // {@link mActionListDelegate} should be set in {@code ExtensionActionListMediator}'s
        // constructor.
        assert mActionListDelegate != null;

        mActionListDelegate.hideActivePopup();
    }

    @CalledByNative
    public boolean hasActivePopup() {
        // {@link mActionListDelegate} should be set in {@code ExtensionActionListMediator}'s
        // constructor.
        assert mActionListDelegate != null;

        return mActionListDelegate.hasActivePopup();
    }

    @CalledByNative
    public void closeExtensionsMenuIfOpen() {
        // {@link mMenuDelegate} should be set in {@code ExtensionsMenuCoordinator}'s constructor.
        assert mMenuDelegate != null;

        mMenuDelegate.closeExtensionsMenuIfOpen();
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
    public void onActiveWebContentsChanged(
            @JniType("content::WebContents*") @Nullable WebContents webContents) {
        for (Observer observer : mObservers) {
            observer.onActiveWebContentsChanged(webContents);
        }
    }

    public interface Observer {
        // Called after all actions are added to the model.
        default void onActionsInitialized() {}

        // Called when an action is added to the model.
        default void onActionAdded(String actionId) {}

        // Called when an action is removed from the model.
        default void onActionRemoved(String actionId) {}

        // Called when an action in the model is updated.
        default void onActionUpdated(String actionId) {}

        // Called when the pinned actions in the model are changed.
        default void onPinnedActionsChanged() {}

        // Called when the active web contents changes due to e.g. navigation or tab change.
        default void onActiveWebContentsChanged(@Nullable WebContents webContents) {}

        // Called when the request access button parameters have changed.
        default void onRequestAccessButtonParamsChanged() {}

        // Called when both the extensions button and the request access button should be updated.
        default void onToolbarControlStateUpdated() {}

        // Called when the manage extensions IPH should be shown.
        default void showManageExtensionsIPH() {}

        // Called when the pinned by default IPH should be shown.
        default void showPinnedByDefaultIPH(String extensionId) {}
    }

    public interface ActionListDelegate {
        // Called when the popup should be shown.
        void triggerPopup(
                String actionId,
                ExtensionActionPopupContents popupContents,
                boolean inspectWithDevTools);

        // Called when the context menu should be shown.
        void showContextMenu(String actionId);

        // Returns whether there is a popped out action.
        boolean hasPoppedOutAction();

        // Called when active popup should be hidden.
        void hideActivePopup();

        // Returns whether there is an active popup.
        boolean hasActivePopup();
    }

    public interface MenuDelegate {
        // Closes the extensions menu if it was open.
        void closeExtensionsMenuIfOpen();
    }

    @NativeMethods
    interface Natives {
        JniUniquePtr<NativeExtensionsToolbarAndroid> init(
                ExtensionsToolbarBridge bridge, JniPtr<NativeBrowserWindowInterface> browserWindow);

        @Nullable ExtensionAction getAction(
                JniPtr<NativeExtensionsToolbarAndroid> self,
                @JniType("std::string") String actionId,
                @JniType("content::WebContents*") @Nullable WebContents webContents);

        @JniType("SkBitmap")
        @Nullable Bitmap getIcon(
                JniPtr<NativeExtensionsToolbarAndroid> self,
                @JniType("std::string") String actionId,
                @JniType("content::WebContents*") @Nullable WebContents webContents,
                int canvasWidthDp,
                int canvasHeightDp,
                float scaleFactor);

        @JniType("std::vector<std::string>")
        String[] getAllActionIds(JniPtr<NativeExtensionsToolbarAndroid> self);

        @JniType("std::vector<std::string>")
        String[] getPinnedActionIds(JniPtr<NativeExtensionsToolbarAndroid> self);

        boolean isActionDraggable(
                JniPtr<NativeExtensionsToolbarAndroid> self,
                @JniType("std::string") String actionId);

        void executeUserAction(
                JniPtr<NativeExtensionsToolbarAndroid> self,
                @JniType("std::string") String actionId,
                @JniType("ToolbarActionViewModel::InvocationSource") int source);

        void movePinnedAction(
                JniPtr<NativeExtensionsToolbarAndroid> self,
                @JniType("std::string") String actionId,
                int targetIndex);

        boolean onRequestAccessButtonClicked(
                JniPtr<NativeExtensionsToolbarAndroid> self,
                @JniType("content::WebContents*") WebContents webContents);

        RequestAccessButtonParams getRequestAccessButtonParams(
                JniPtr<NativeExtensionsToolbarAndroid> self,
                @JniType("content::WebContents*") WebContents webContents);

        boolean handleKeyDownEvent(
                JniPtr<NativeExtensionsToolbarAndroid> self,
                @JniType("ui::KeyEventAndroid") KeyEvent keyEvent);

        ExtensionsMenuButtonState getMenuButtonState(
                JniPtr<NativeExtensionsToolbarAndroid> self,
                @JniType("content::WebContents*") WebContents webContents,
                int canvasWidthDp,
                int canvasHeightDp,
                float scaleFactor,
                int color);
    }
}
