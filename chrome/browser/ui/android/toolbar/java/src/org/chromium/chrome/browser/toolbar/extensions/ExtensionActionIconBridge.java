// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.graphics.Bitmap;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.LifetimeAssert;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.profiles.Profile;

/** A JNI bridge providing access to extension action icons. */
public class ExtensionActionIconBridge implements Destroyable {
    private long mNativeExtensionActionIconBridge;
    @NonNull private final Observer mObserver;

    private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    /**
     * Constructs a bridge for the specified profile and the action.
     *
     * <p>An instance must be destroyed with {@link #destroy()} when you are done with it.
     */
    public ExtensionActionIconBridge(
            @NonNull Profile profile, @NonNull String actionId, @NonNull Observer observer) {
        mObserver = observer;
        mNativeExtensionActionIconBridge =
                ExtensionActionIconBridgeJni.get().init(this, profile, actionId);
        assert mNativeExtensionActionIconBridge != 0;
    }

    /** Destroys this bridge. */
    @Override
    public void destroy() {
        assert mNativeExtensionActionIconBridge != 0;
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
        ExtensionActionIconBridgeJni.get().destroy(mNativeExtensionActionIconBridge);
        mNativeExtensionActionIconBridge = 0;
    }

    /**
     * Returns the icon for the action in the specified tab.
     *
     * <p>While loading the icon, this method returns a transparent icon.
     */
    public Bitmap getIcon(int tabId) {
        assert mNativeExtensionActionIconBridge != 0;
        return ExtensionActionIconBridgeJni.get().getIcon(mNativeExtensionActionIconBridge, tabId);
    }

    @CalledByNative
    private void onIconUpdated() {
        assert mNativeExtensionActionIconBridge != 0;
        mObserver.onIconUpdated(this);
    }

    public interface Observer {
        /** Notifies that the icon has been updated. */
        void onIconUpdated(ExtensionActionIconBridge bridge);
    }

    @NativeMethods
    public interface Natives {
        long init(
                ExtensionActionIconBridge self,
                @JniType("Profile*") Profile profile,
                @JniType("std::string") String actionId);

        void destroy(long nativeExtensionActionIconBridge);

        Bitmap getIcon(long nativeExtensionActionIconBridge, int tabId);
    }
}
