// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions.side_panel;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature.InitInfo;

/** Implements {@link ExtensionSidePanelManagerBridge}. */
@NullMarked
final class ExtensionSidePanelManagerBridgeImpl implements ExtensionSidePanelManagerBridge {
    private long mNativeExtensionSidePanelManagerBridge;

    ExtensionSidePanelManagerBridgeImpl() {}

    @Override
    public void onAddedToTask(InitInfo initInfo) {
        assert mNativeExtensionSidePanelManagerBridge == 0
                : "ExtensionSidePanelManagerBridge is already added to a task.";

        mNativeExtensionSidePanelManagerBridge =
                ExtensionSidePanelManagerBridgeImplJni.get()
                        .create(/* caller= */ this, initInfo.nativeBrowserWindowPtr);
    }

    @Override
    public void onFeatureRemoved() {
        destroyNativeExtensionSidePanelManagerBridge();
    }

    private void destroyNativeExtensionSidePanelManagerBridge() {
        if (mNativeExtensionSidePanelManagerBridge != 0) {
            ExtensionSidePanelManagerBridgeImplJni.get()
                    .destroy(mNativeExtensionSidePanelManagerBridge);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeExtensionSidePanelManagerBridge = 0;
    }

    @NativeMethods
    interface Natives {
        long create(ExtensionSidePanelManagerBridgeImpl caller, long nativeBrowserWindowPtr);

        void destroy(long nativeExtensionSidePanelManagerBridge);
    }
}
