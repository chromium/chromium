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

    /** Returns the extension site permissions state from native. */
    public ExtensionsMenuTypes.ExtensionSitePermissionsState getExtensionSitePermissionsState(
            String extensionId) {
        return ExtensionsMenuBridgeJni.get()
                .getExtensionSitePermissionsState(
                        mNativeExtensionsMenuDelegateAndroid, extensionId);
    }

    /** Returns the list of menu entries with their states from native. */
    public List<ExtensionsMenuTypes.MenuEntryState> getMenuEntries() {
        return ExtensionsMenuBridgeJni.get().getMenuEntries(mNativeExtensionsMenuDelegateAndroid);
    }

    /**
     * Returns the menu entry state for the given extension index from native.
     *
     * @param actionIndex The index of the extension in the menu.
     */
    public ExtensionsMenuTypes.MenuEntryState getMenuEntry(int actionIndex) {
        return ExtensionsMenuBridgeJni.get()
                .getMenuEntry(mNativeExtensionsMenuDelegateAndroid, actionIndex);
    }

    /** Returns the site settings state from native. */
    public ExtensionsMenuTypes.SiteSettingsState getSiteSettingsState() {
        return ExtensionsMenuBridgeJni.get().getSiteSettings(mNativeExtensionsMenuDelegateAndroid);
    }

    /**
     * Called when a site access option for an extension is selected in the UI.
     *
     * @param extensionId The ID of the extension.
     * @param siteAccess The selected site access option.
     */
    public void onExtensionSiteAccessSelected(
            String extensionId, @ExtensionsMenuTypes.UserSiteAccess int siteAccess) {
        ExtensionsMenuBridgeJni.get()
                .onSiteAccessSelected(
                        mNativeExtensionsMenuDelegateAndroid, extensionId, siteAccess);
    }

    /**
     * Called when the site access toggle for an extension is changed in the UI.
     *
     * @param extensionId The ID of the extension.
     * @param isOn Whether the toggle is on.
     */
    public void onExtensionToggleSelected(String extensionId, boolean isOn) {
        ExtensionsMenuBridgeJni.get()
                .onExtensionToggleSelected(mNativeExtensionsMenuDelegateAndroid, extensionId, isOn);
    }

    /**
     * Called when the show requests toggle for an extension is changed in the UI.
     *
     * @param extensionId The ID of the extension.
     * @param isOn Whether the toggle is on.
     */
    public void onShowRequestsTogglePressed(String extensionId, boolean isOn) {
        ExtensionsMenuBridgeJni.get()
                .onShowRequestsTogglePressed(
                        mNativeExtensionsMenuDelegateAndroid, extensionId, isOn);
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

    /** Returns the optional section to display in the menu. */
    public int getOptionalSection() {
        return ExtensionsMenuBridgeJni.get()
                .getOptionalSection(mNativeExtensionsMenuDelegateAndroid);
    }

    /** Returns the list of host access requests. */
    public List<ExtensionsMenuTypes.HostAccessRequest> getHostAccessRequests() {
        return ExtensionsMenuBridgeJni.get()
                .getHostAccessRequests(mNativeExtensionsMenuDelegateAndroid);
    }

    /** Called when a host access request for `extension_id` is allowed. */
    public void onAllowExtensionClicked(String extensionId) {
        ExtensionsMenuBridgeJni.get()
                .onAllowExtensionClicked(mNativeExtensionsMenuDelegateAndroid, extensionId);
    }

    /** Called when a host access request for `extension_id` is dismissed. */
    public void onDismissExtensionClicked(String extensionId) {
        ExtensionsMenuBridgeJni.get()
                .onDismissExtensionClicked(mNativeExtensionsMenuDelegateAndroid, extensionId);
    }

    /** Called when the reload page button is clicked. */
    public void onReloadPageButtonClicked() {
        ExtensionsMenuBridgeJni.get()
                .onReloadPageButtonClicked(mNativeExtensionsMenuDelegateAndroid);
    }

    /** Returns whether the native menu model is ready. */
    public boolean isReady() {
        return ExtensionsMenuBridgeJni.get().isReady(mNativeExtensionsMenuDelegateAndroid);
    }

    /**
     * Callback from native indicating that an action has been added.
     *
     * @param actionIndex The index of the menu entry in the menu corresponding to the action added.
     */
    @CalledByNative
    public void onActionAdded(int actionIndex) {
        mObserver.onActionAdded(actionIndex);
    }

    /**
     * Callback from native indicating that an extension icon has been updated.
     *
     * @param actionIndex The index of the menu entry in the menu corresponding to the action
     *     updated.
     */
    @CalledByNative
    public void onActionIconUpdated(int actionIndex) {
        mObserver.onActionIconUpdated(actionIndex);
    }

    /**
     * Callback from native indicating that an action has been removed.
     *
     * @param actionIndex The index of the menu entry in the menu corresponding to the action
     *     removed.
     */
    @CalledByNative
    public void onActionRemoved(int actionIndex) {
        mObserver.onActionRemoved(actionIndex);
    }

    /**
     * Callback from native indicating that an extension has been updated.
     *
     * @param actionIndex The index of the menu entry in the menu corresponding to the action
     *     updated.
     */
    @CalledByNative
    public void onActionUpdated(int actionIndex) {
        mObserver.onActionUpdated(actionIndex);
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

    @CalledByNative
    private void onHostAccessRequestAdded(@JniType("std::string") String extensionId) {
        mObserver.onHostAccessRequestAdded(extensionId);
    }

    @CalledByNative
    private void onHostAccessRequestUpdated(@JniType("std::string") String extensionId) {
        mObserver.onHostAccessRequestUpdated(extensionId);
    }

    @CalledByNative
    private void onHostAccessRequestRemoved(@JniType("std::string") String extensionId) {
        mObserver.onHostAccessRequestRemoved(extensionId);
    }

    @CalledByNative
    private void onHostAccessRequestsCleared() {
        mObserver.onHostAccessRequestsCleared();
    }

    public interface Observer {
        /** Called when an extension icon has been updated on actionIndex. */
        void onActionIconUpdated(int actionIndex);

        /** Called when an action has been added to the menu. */
        void onActionAdded(int actionIndex);

        /** Called when an action has been removed from the menu. */
        void onActionRemoved(int actionIndex);

        /** Called when an extension has been updated on actionIndex. */
        void onActionUpdated(int actionIndex);

        /** Called when the menu data is ready to be consumed. */
        void onReady();

        /**
         * Called when a major event in the native model has occurred, potentially affecting
         * multiple UI properties. The Mediator should pull the necessary data to refresh the
         * current view.
         */
        void onModelChanged();

        /** Called when a host access request has been added. */
        void onHostAccessRequestAdded(String extensionId);

        /** Called when a host access request has been updated. */
        void onHostAccessRequestUpdated(String extensionId);

        /** Called when a host access request has been removed. */
        void onHostAccessRequestRemoved(String extensionId);

        /** Called when all host access requests have been cleared. */
        void onHostAccessRequestsCleared();
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

        /** Returns the extension site permissions state from native. */
        ExtensionsMenuTypes.ExtensionSitePermissionsState getExtensionSitePermissionsState(
                long nativeExtensionsMenuDelegateAndroid,
                @JniType("std::string") String extensionId);

        /** Returns the list of menu entries with their states from native. */
        @JniType("std::vector<base::android::ScopedJavaLocalRef<jobject>>")
        List<ExtensionsMenuTypes.MenuEntryState> getMenuEntries(
                long nativeExtensionsMenuDelegateAndroid);

        /**
         * Returns the menu entry state corresponding to the extension at actionIndex from native.
         */
        ExtensionsMenuTypes.MenuEntryState getMenuEntry(
                long nativeExtensionsMenuDelegateAndroid, int actionIndex);

        /** Returns the optional section to display in the menu. */
        int getOptionalSection(long nativeExtensionsMenuDelegateAndroid);

        /** Called when a site access option for an extension is selected in the UI. */
        void onSiteAccessSelected(
                long nativeExtensionsMenuDelegateAndroid,
                @JniType("std::string") String extensionId,
                @JniType("extensions::PermissionsManager::UserSiteAccess") int siteAccess);

        /** Called when the site access toggle for an extension is changed in the UI. */
        void onExtensionToggleSelected(
                long nativeExtensionsMenuDelegateAndroid,
                @JniType("std::string") String extensionId,
                boolean isOn);

        /** Returns the list of host access requests. */
        @JniType("std::vector<base::android::ScopedJavaLocalRef<jobject>>")
        List<ExtensionsMenuTypes.HostAccessRequest> getHostAccessRequests(
                long nativeExtensionsMenuDelegateAndroid);

        /** Called when a host access request for `extension_id` is allowed. */
        void onAllowExtensionClicked(
                long nativeExtensionsMenuDelegateAndroid,
                @JniType("std::string") String extensionId);

        /** Called when a host access request for `extension_id` is dismissed. */
        void onDismissExtensionClicked(
                long nativeExtensionsMenuDelegateAndroid,
                @JniType("std::string") String extensionId);

        /** Tells the native model to reload the page. */
        void onReloadPageButtonClicked(long nativeExtensionsMenuDelegateAndroid);

        /** Called when the show requests toggle for an extension is changed in the UI. */
        void onShowRequestsTogglePressed(
                long nativeExtensionsMenuDelegateAndroid,
                @JniType("std::string") String extensionId,
                boolean isOn);

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
