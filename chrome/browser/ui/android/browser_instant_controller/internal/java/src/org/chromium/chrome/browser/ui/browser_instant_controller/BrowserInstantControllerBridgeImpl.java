// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_instant_controller;

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

/** Implements {@link BrowserInstantControllerBridge}. */
@NullMarked
final class BrowserInstantControllerBridgeImpl implements BrowserInstantControllerBridge {
    @JniType("::BrowserInstantControllerBridge")
    interface NativeBrowserInstantControllerBridge extends JniTypeToken {}

    private @Nullable JniUniquePtr<NativeBrowserInstantControllerBridge> mNative;

    BrowserInstantControllerBridgeImpl() {}

    @Override
    public void onAddedToTask(InitInfo initInfo) {
        ThreadUtils.assertOnUiThread();
        assert mNative == null : "BrowserInstantControllerBridge is already added to a task.";

        if (initInfo.nativeBrowserWindow == null) {
            // There is no browser window for the (profile, activity) this feature was keyed on, so
            // there are no tabs to reload.
            return;
        }

        mNative = BrowserInstantControllerBridgeImplJni.get().create(initInfo.nativeBrowserWindow);
    }

    @Override
    public void onFeatureRemoved() {
        ThreadUtils.assertOnUiThread();
        if (mNative != null) {
            mNative.destroy();
            mNative = null;
        }
    }

    @NativeMethods
    interface Natives {
        /**
         * Creates a native {@code BrowserInstantControllerBridge} for {@code browserWindow}.
         *
         * @param browserWindow The native {@code BrowserWindowInterface} whose New Tab Pages are
         *     reloaded.
         */
        JniUniquePtr<NativeBrowserInstantControllerBridge> create(
                JniPtr<NativeBrowserWindowInterface> browserWindow);
    }
}
