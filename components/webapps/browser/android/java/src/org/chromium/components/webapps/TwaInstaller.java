// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;

/**
 * Provides functionality related to installing auto-minted TWA for its C++ counterpart,
 * twa_installer.cc.
 */
@JNINamespace("webapps")
@NullMarked
class TwaInstaller {
    private static final String TAG = "TwaInstaller";

    private final long mNativeInstaller;

    TwaInstaller(long nativeInstaller) {
        mNativeInstaller = nativeInstaller;
    }

    @CalledByNative
    private static boolean start(
            long nativeInstaller,
            @JniType("std::u16string") String title,
            @JniType("std::string") String manifestUrl) {
        return new TwaInstaller(nativeInstaller).start(title, manifestUrl);
    }

    private boolean start(String title, String manifestUrl) {
        if (manifestUrl.isEmpty()) {
            Log.e(TAG, "Invalid manifest URL");
            onFlowCompleted(AddToHomescreenEvent.INSTALL_FAILED);
            return false;
        }

        var aconfigFlaggedApiDelegate = AconfigFlaggedApiDelegate.getInstance();
        if (aconfigFlaggedApiDelegate == null) {
            Log.e(TAG, "Failed to get AconfigFlaggedApiDelegate to call installTwa()");
            onFlowCompleted(AddToHomescreenEvent.INSTALL_FAILED);
            return false;
        }

        // TODO(crbug.com/468477882): Consider if we should also report INSTALL_STARTED and UI_SHOWN
        // events at some appropriate timing.
        if (!aconfigFlaggedApiDelegate.installTwa(
                title,
                manifestUrl,
                /* installSucceededCallback= */ () -> {
                    onFlowCompleted(AddToHomescreenEvent.INSTALL_REQUEST_FINISHED);
                },
                /* installFailedCallback= */ () -> {
                    onFlowCompleted(AddToHomescreenEvent.INSTALL_FAILED);
                },
                /* installCancelledCallback= */ () -> {
                    onFlowCompleted(AddToHomescreenEvent.UI_CANCELLED);
                })) {
            Log.e(TAG, "Failed to call installTwa()");
            return false;
        }

        return true;
    }

    private void onFlowCompleted(@AddToHomescreenEvent int event) {
        TwaInstallerJni.get().onInstallEvent(mNativeInstaller, event);
        TwaInstallerJni.get().destroy(mNativeInstaller);
    }

    @NativeMethods
    interface Natives {
        void onInstallEvent(long nativeTwaInstaller, int event);

        void destroy(long nativeTwaInstaller);
    }
}
