// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.side_panel;

import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask.NativeBrowserWindowInterface;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature.InitInfo;

/** Implements {@link ExtensionSidePanelManagerBridge}. */
@NullMarked
final class ExtensionSidePanelManagerBridgeImpl implements ExtensionSidePanelManagerBridge {
    @JniType("::ExtensionSidePanelManagerBridge")
    interface NativeExtensionSidePanelManagerBridge extends JniTypeToken {}

    private @Nullable JniUniquePtr<NativeExtensionSidePanelManagerBridge> mNative;

    ExtensionSidePanelManagerBridgeImpl() {}

    @Override
    public void onAddedToTask(InitInfo initInfo) {
        ThreadUtils.assertOnUiThread();
        assert mNative == null : "ExtensionSidePanelManagerBridge is already added to a task.";

        mNative = ExtensionSidePanelManagerBridgeImplJni.get().create(initInfo.nativeBrowserWindow);
    }

    @Override
    public void onFeatureRemoved() {
        ThreadUtils.assertOnUiThread();
        destroyNativeExtensionSidePanelManagerBridge();
    }

    private void destroyNativeExtensionSidePanelManagerBridge() {
        if (mNative != null) {
            mNative.destroy();
            mNative = null;
        }
    }

    @NativeMethods
    interface Natives {
        JniUniquePtr<NativeExtensionSidePanelManagerBridge> create(
                @Nullable JniPtr<NativeBrowserWindowInterface> browserWindow);
    }
}
