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
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;

/** A JNI bridge that provides native extensions menu data to the Java UI. */
@NullMarked
@JNINamespace("extensions")
public class ExtensionsMenuBridge implements Destroyable {
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private long mNativeExtensionsMenuDelegateAndroid;
    private final Observer mObserver;

    public ExtensionsMenuBridge(ChromeAndroidTask task, Observer observer) {
        mObserver = observer;
        mNativeExtensionsMenuDelegateAndroid =
                ExtensionsMenuBridgeJni.get().init(this, task.getOrCreateNativeBrowserWindowPtr());
    }

    @Override
    public void destroy() {
        assert mNativeExtensionsMenuDelegateAndroid != 0;
        ExtensionsMenuBridgeJni.get().destroy(mNativeExtensionsMenuDelegateAndroid);
        mNativeExtensionsMenuDelegateAndroid = 0;
        LifetimeAssert.destroy(mLifetimeAssert);
    }

    /** Returns a flattened list of action IDs and names from native. */
    public String[] getActions() {
        return ExtensionsMenuBridgeJni.get().getActions(mNativeExtensionsMenuDelegateAndroid);
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
        long init(ExtensionsMenuBridge bridge, long browserWindowInterfacePtr);

        void destroy(long nativeExtensionsMenuDelegateAndroid);

        @JniType("std::vector<std::string>")
        String[] getActions(long nativeExtensionsMenuDelegateAndroid);

        boolean isReady(long nativeExtensionsMenuDelegateAndroid);
    }
}
