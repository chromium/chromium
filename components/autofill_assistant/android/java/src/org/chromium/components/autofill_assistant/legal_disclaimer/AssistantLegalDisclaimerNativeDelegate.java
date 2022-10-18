// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.legal_disclaimer;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/** Native Delegate for the AssistantLegalDisclaimerNativeDelegate. */
@JNINamespace("autofill_assistant")
public class AssistantLegalDisclaimerNativeDelegate implements AssistantLegalDisclaimerDelegate {
    private long mNativeAssistantLegalDisclaimerNativeDelegate;

    @CalledByNative
    private AssistantLegalDisclaimerNativeDelegate(
            long nativeAssistantLegalDisclaimerNativeDelegate) {
        mNativeAssistantLegalDisclaimerNativeDelegate =
                nativeAssistantLegalDisclaimerNativeDelegate;
    }

    @Override
    public void onLinkClicked(int link) {
        if (mNativeAssistantLegalDisclaimerNativeDelegate != 0) {
            AssistantLegalDisclaimerNativeDelegateJni.get().onLinkClicked(
                    mNativeAssistantLegalDisclaimerNativeDelegate,
                    AssistantLegalDisclaimerNativeDelegate.this, link);
        }
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeAssistantLegalDisclaimerNativeDelegate = 0;
    }

    @NativeMethods
    interface Natives {
        void onLinkClicked(long nativeAssistantLegalDisclaimerNativeDelegate,
                AssistantLegalDisclaimerNativeDelegate caller, int link);
    }
}
