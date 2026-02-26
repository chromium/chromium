// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.graphics.Bitmap;

import androidx.annotation.IntDef;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.toolbar.InvocationSource;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A JNI bridge to interact with extension actions for the toolbar. */
@NullMarked
@JNINamespace("extensions")
public class ExtensionsToolbarBridge implements Destroyable {
    // TODO(crbug.com/423483658): Consider moving ExtensionsMenuButtonState and related types
    // (e.g., RequestAccessButtonParams) into a new ExtensionControls.java file.
    @IntDef({
        ExtensionsMenuButtonState.ALL_EXTENSIONS_BLOCKED,
        ExtensionsMenuButtonState.ANY_EXTENSION_HAS_ACCESS,
        ExtensionsMenuButtonState.DEFAULT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ExtensionsMenuButtonState {
        int ALL_EXTENSIONS_BLOCKED = 0;
        int ANY_EXTENSION_HAS_ACCESS = 1;
        int DEFAULT = 2;
    }

    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private long mNativeExtensionsToolbarAndroid;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final Profile mProfile;

    // The delegate is set via a setter because of a bidirectional dependency
    // with {@code ExtensionActionListMediator}.
    private @Nullable Delegate mDelegate;

    public ExtensionsToolbarBridge(ChromeAndroidTask task, Profile profile) {
        mProfile = profile;
        mNativeExtensionsToolbarAndroid =
                ExtensionsToolbarBridgeJni.get()
                        .init(this, task.getOrCreateNativeBrowserWindowPtr(profile));
    }

    @Override
    public void destroy() {
        assert mNativeExtensionsToolbarAndroid != 0;
        ExtensionsToolbarBridgeJni.get().destroy(mNativeExtensionsToolbarAndroid);
        mNativeExtensionsToolbarAndroid = 0;
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
        if (mProfile.shutdownStarted()) {
            // TODO(crbug.com/459079170): This is to prevent tests from breaking. {@code
            // ExtensionToolbarCoordinatorImpl} should ideally be destroyed following {@code
            // ChromeAndroidTask}'s destruction, and it is currently being worked on.
            return null;
        }
        return ExtensionsToolbarBridgeJni.get()
                .getAction(mNativeExtensionsToolbarAndroid, actionId);
    }

    @Nullable
    public Bitmap getIcon(
            String actionId,
            @Nullable WebContents webContents,
            int canvasWidthDp,
            int canvasHeightDp,
            float scaleFactor) {
        if (mProfile.shutdownStarted()) {
            // TODO(crbug.com/459079170): This is to prevent tests from breaking. {@code
            // ExtensionToolbarCoordinatorImpl} should ideally be destroyed following {@code
            // ChromeAndroidTask}'s destruction, and it is currently being worked on.
            return null;
        }
        return ExtensionsToolbarBridgeJni.get()
                .getIcon(
                        mNativeExtensionsToolbarAndroid,
                        actionId,
                        webContents,
                        canvasWidthDp,
                        canvasHeightDp,
                        scaleFactor);
    }

    public String[] getAllActionIds() {
        if (mProfile.shutdownStarted()) {
            // TODO(crbug.com/459079170): This is to prevent tests from breaking. {@code
            // ExtensionToolbarCoordinatorImpl} should ideally be destroyed following {@code
            // ChromeAndroidTask}'s destruction, and it is currently being worked on.
            return new String[0];
        }
        return ExtensionsToolbarBridgeJni.get().getAllActionIds(mNativeExtensionsToolbarAndroid);
    }

    public String[] getPinnedActionIds() {
        if (mProfile.shutdownStarted()) {
            // TODO(crbug.com/459079170): This is to prevent tests from breaking. {@code
            // ExtensionToolbarCoordinatorImpl} should ideally be destroyed following {@code
            // ChromeAndroidTask}'s destruction, and it is currently being worked on.
            return new String[0];
        }
        return ExtensionsToolbarBridgeJni.get().getPinnedActionIds(mNativeExtensionsToolbarAndroid);
    }

    public void executeUserAction(String actionId, @InvocationSource int source) {
        if (mProfile.shutdownStarted()) {
            // TODO(crbug.com/459079170): This is to prevent tests from breaking. {@code
            // ExtensionToolbarCoordinatorImpl} should ideally be destroyed following {@code
            // ChromeAndroidTask}'s destruction, and it is currently being worked on.
            return;
        }
        ExtensionsToolbarBridgeJni.get()
                .executeUserAction(mNativeExtensionsToolbarAndroid, actionId, source);
    }

    public void movePinnedAction(String actionId, int targetIndex) {
        if (mProfile.shutdownStarted()) {
            // TODO(crbug.com/459079170): This is to prevent tests from breaking. {@code
            // ExtensionToolbarCoordinatorImpl} should ideally be destroyed following {@code
            // ChromeAndroidTask}'s destruction, and it is currently being worked on.
            return;
        }
        ExtensionsToolbarBridgeJni.get()
                .movePinnedAction(mNativeExtensionsToolbarAndroid, actionId, targetIndex);
    }

    public @ExtensionsMenuButtonState int getExtensionsMenuButtonState(WebContents webContents) {
        assert mNativeExtensionsToolbarAndroid != 0;
        return ExtensionsToolbarBridgeJni.get()
                .getExtensionsMenuButtonState(mNativeExtensionsToolbarAndroid, webContents);
    }

    public RequestAccessButtonParams getRequestAccessButtonParams(WebContents webContents) {
        assert mNativeExtensionsToolbarAndroid != 0;
        RequestAccessButtonParams params =
                ExtensionsToolbarBridgeJni.get()
                        .getRequestAccessButtonParams(mNativeExtensionsToolbarAndroid, webContents);
        assert params != null;
        return params;
    }

    @CalledByNative
    public void triggerPopup(@JniType("std::string") String actionId, long nativeHostPtr) {
        // {@link mDelegate} should be set in {@code ExtensionActionListMediator}'s constructor.
        assert mDelegate != null;

        mDelegate.triggerPopup(actionId, nativeHostPtr);
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
        default void onActiveWebContentsChanged() {}

        // Called when the request access button parameters have changed.
        default void onRequestAccessButtonParamsChanged() {}

        // Called when both the extensions button and the request access button should be updated.
        default void onToolbarControlStateUpdated() {}
    }

    public interface Delegate {
        // Called when the popup should be shown.
        void triggerPopup(String actionId, long nativeHostPtr);
    }

    @NativeMethods
    public interface Natives {
        long init(ExtensionsToolbarBridge bridge, long browserWindowInterfacePtr);

        void destroy(long nativeExtensionsToolbarAndroid);

        @Nullable ExtensionAction getAction(
                long nativeExtensionsToolbarAndroid, @JniType("std::string") String actionId);

        @Nullable Bitmap getIcon(
                long nativeExtensionsToolbarAndroid,
                @JniType("std::string") String actionId,
                @Nullable @JniType("content::WebContents*") WebContents webContents,
                int canvasWidthDp,
                int canvasHeightDp,
                float scaleFactor);

        @JniType("std::vector<std::string>")
        String[] getAllActionIds(long nativeExtensionsToolbarAndroid);

        @JniType("std::vector<std::string>")
        String[] getPinnedActionIds(long nativeExtensionsToolbarAndroid);

        void executeUserAction(
                long nativeExtensionsToolbarAndroid,
                @JniType("std::string") String actionId,
                @JniType("ToolbarActionViewModel::InvocationSource") int source);

        void movePinnedAction(
                long nativeExtensionsToolbarAndroid,
                @JniType("std::string") String actionId,
                int targetIndex);

        RequestAccessButtonParams getRequestAccessButtonParams(
                long nativeExtensionsToolbarAndroid,
                @JniType("content::WebContents*") WebContents webContents);

        int getExtensionsMenuButtonState(
                long nativeExtensionsToolbarAndroid,
                @JniType("content::WebContents*") WebContents webContents);
    }
}
