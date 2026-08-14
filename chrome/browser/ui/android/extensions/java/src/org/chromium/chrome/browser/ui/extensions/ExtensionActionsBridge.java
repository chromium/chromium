// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;

/** A JNI bridge to interact with extension actions for the toolbar. */
@NullMarked
@JNINamespace("extensions")
public class ExtensionActionsBridge implements Destroyable {
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private long mNativeExtensionActionsBridge;

    public ExtensionActionsBridge(ChromeAndroidTask task, Profile profile) {
        mNativeExtensionActionsBridge =
                ExtensionActionsBridgeJni.get()
                        .init(this, task.getOrCreateNativeBrowserWindowPtr(profile));
    }

    @Override
    public void destroy() {
        assert mNativeExtensionActionsBridge != 0;
        ExtensionActionsBridgeJni.get().destroy(mNativeExtensionActionsBridge);
        mNativeExtensionActionsBridge = 0;
        LifetimeAssert.destroy(mLifetimeAssert);
    }

    @NativeMethods
    public interface Natives {

        long init(ExtensionActionsBridge bridge, long browserWindowInterfacePtr);

        void destroy(long nativeExtensionActionsBridge);
    }
}
