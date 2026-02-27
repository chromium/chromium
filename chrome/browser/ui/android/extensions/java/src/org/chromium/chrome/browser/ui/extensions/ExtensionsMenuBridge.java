// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;

import java.util.List;

/** A JNI bridge that provides native extensions menu data to the Java UI. */
@NullMarked
@JNINamespace("extensions")
public class ExtensionsMenuBridge implements Destroyable {
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    private long mNativeExtensionsMenuDelegateAndroid;
    private final Observer mObserver;

    public ExtensionsMenuBridge(ChromeAndroidTask task, Profile profile, Observer observer) {
        mObserver = observer;
        mNativeExtensionsMenuDelegateAndroid =
                ExtensionsMenuBridgeJni.get()
                        .init(this, task.getOrCreateNativeBrowserWindowPtr(profile));
    }

    @Override
    public void destroy() {
        assert mNativeExtensionsMenuDelegateAndroid != 0;
        ExtensionsMenuBridgeJni.get().destroy(mNativeExtensionsMenuDelegateAndroid);
        mNativeExtensionsMenuDelegateAndroid = 0;
        LifetimeAssert.destroy(mLifetimeAssert);
    }

    /** Returns the icon for the given extension index from native. */
    public @Nullable Bitmap getActionIcon(int actionIndex) {
        return ExtensionsMenuBridgeJni.get()
                .getActionIcon(mNativeExtensionsMenuDelegateAndroid, actionIndex);
    }

    /** Returns the list of menu entries with their states from native. */
    public List<ExtensionsMenuTypes.MenuEntryState> getMenuEntries() {
        return ExtensionsMenuBridgeJni.get().getMenuEntries(mNativeExtensionsMenuDelegateAndroid);
    }

    /** Returns the site settings state from native. */
    public ExtensionsMenuTypes.SiteSettingsState getSiteSettingsState() {
        return ExtensionsMenuBridgeJni.get().getSiteSettings(mNativeExtensionsMenuDelegateAndroid);
    }

    /**
     * Called when the site settings toggle is changed in the UI.
     *
     * @param isChecked Whether the toggle is checked.
     */
    public void onSiteSettingsToggleChanged(boolean isChecked) {
        ExtensionsMenuBridgeJni.get()
                .onSiteSettingsToggleChanged(mNativeExtensionsMenuDelegateAndroid, isChecked);
    }

    /** Returns whether the native menu model is ready. */
    public boolean isReady() {
        return ExtensionsMenuBridgeJni.get().isReady(mNativeExtensionsMenuDelegateAndroid);
    }

    /**
     * Callback from native indicating that an extension icon has been updated.
     *
     * @param actionIndex The index of the updated extension in the menu.
     */
    @CalledByNative
    public void onActionIconUpdated(int actionIndex) {
        mObserver.onActionIconUpdated(actionIndex);
    }

    /**
     * Callback from native indicating that the menu data is ready. This will not be called if the
     * menu data is ready at the menu bridge initialization.
     */
    @CalledByNative
    public void onReady() {
        mObserver.onReady();
    }

    @CalledByNative
    private void onModelChanged() {
        if (mObserver != null) {
            mObserver.onModelChanged();
        }
    }

    public interface Observer {
        /** Called when an extension icon has been updated on actionIndex. */
        void onActionIconUpdated(int actionIndex);

        /** Called when the menu data is ready to be consumed. */
        void onReady();

        /**
         * Called when a major event in the native model has occurred, potentially affecting
         * multiple UI properties. The Mediator should pull the necessary data to refresh the
         * current view.
         */
        void onModelChanged();
    }

    @NativeMethods
    public interface Natives {
        /**
         * Initializes the native ExtensionsMenuDelegateAndroid and returns its pointer.
         *
         * @param bridge The Java bridge object.
         * @param browserWindowInterfacePtr The pointer to the native BrowserWindowInterface.
         */
        long init(ExtensionsMenuBridge bridge, long browserWindowInterfacePtr);

        /** Destroys the native ExtensionsMenuDelegateAndroid. */
        void destroy(long nativeExtensionsMenuDelegateAndroid);

        // Returns the icon for an extension's action at actionIndex.
        @Nullable Bitmap getActionIcon(long nativeExtensionsMenuDelegateAndroid, int actionIndex);

        /** Returns the list of menu entries with their states from native. */
        @JniType("std::vector<base::android::ScopedJavaLocalRef<jobject>>")
        List<ExtensionsMenuTypes.MenuEntryState> getMenuEntries(
                long nativeExtensionsMenuDelegateAndroid);

        /** Returns whether the native menu model is ready. */
        boolean isReady(long nativeExtensionsMenuDelegateAndroid);

        /** Returns the site settings state from native. */
        ExtensionsMenuTypes.SiteSettingsState getSiteSettings(
                long nativeExtensionsMenuDelegateAndroid);

        /** Called when the site settings toggle is changed in the UI. */
        void onSiteSettingsToggleChanged(
                long nativeExtensionsMenuDelegateAndroid, boolean isChecked);
    }
}
