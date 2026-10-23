// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask.NativeBrowserWindowInterface;

/** A JNI bridge to interact with extension actions for the toolbar. */
@NullMarked
@JNINamespace("extensions")
public class ExtensionActionsBridge implements Destroyable {
    @JniType("::extensions::ExtensionActionsBridge")
    interface NativeExtensionActionsBridge extends JniTypeToken {}

    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private final JniUniquePtr<NativeExtensionActionsBridge> mNative;

    public ExtensionActionsBridge(ChromeAndroidTask task, Profile profile) {
        mNative =
                ExtensionActionsBridgeJni.get()
                        .init(this, task.getOrCreateNativeBrowserWindow(profile));
    }

    @Override
    public void destroy() {
        mNative.destroy();
        LifetimeAssert.destroy(mLifetimeAssert);
    }

    @NativeMethods
    interface Natives {

        JniUniquePtr<NativeExtensionActionsBridge> init(
                ExtensionActionsBridge bridge, JniPtr<NativeBrowserWindowInterface> browserWindow);
    }
}
