// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

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

    /** Returns the list of menu entries with their states from native. */
    public List<ExtensionsMenuTypes.MenuEntryState> getMenuEntries() {
        return ExtensionsMenuBridgeJni.get().getMenuEntries(mNativeExtensionsMenuDelegateAndroid);
    }

    /** Returns whether the native menu model is ready. */
    public boolean isReady() {
        return ExtensionsMenuBridgeJni.get().isReady(mNativeExtensionsMenuDelegateAndroid);
    }

    /**
     * Callback from native indicating that the menu data is ready. This will not be called if the
     * menu data is ready at the menu bridge initialization.
     */
    @CalledByNative
    public void onReady() {
        mObserver.onReady();
    }

    public interface Observer {
        /** Called when the menu data is ready to be consumed. */
        void onReady();
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

        /** Returns the list of menu entries with their states from native. */
        @JniType("std::vector<base::android::ScopedJavaLocalRef<jobject>>")
        List<ExtensionsMenuTypes.MenuEntryState> getMenuEntries(
                long nativeExtensionsMenuDelegateAndroid);

        /** Returns whether the native menu model is ready. */
        boolean isReady(long nativeExtensionsMenuDelegateAndroid);
    }
}
