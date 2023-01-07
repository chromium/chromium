// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.overlay;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/** Delegate for the overlay. */
@JNINamespace("autofill_assistant")
class AssistantOverlayDelegate {
    private long mNativeAssistantOverlayDelegate;

    @CalledByNative
    private static AssistantOverlayDelegate create(long nativeAssistantOverlayDelegate) {
        return new AssistantOverlayDelegate(nativeAssistantOverlayDelegate);
    }

    private AssistantOverlayDelegate(long nativeAssistantOverlayDelegate) {
        mNativeAssistantOverlayDelegate = nativeAssistantOverlayDelegate;
    }

    /** Called after a certain number of unexpected taps. */
    void onUnexpectedTaps() {
        if (mNativeAssistantOverlayDelegate != 0) {
            AssistantOverlayDelegateJni.get().onUnexpectedTaps(
                    mNativeAssistantOverlayDelegate, AssistantOverlayDelegate.this);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantOverlayDelegate = 0;
    }

    @NativeMethods
    interface Natives {
        void onUnexpectedTaps(long nativeAssistantOverlayDelegate, AssistantOverlayDelegate caller);
    }
}
