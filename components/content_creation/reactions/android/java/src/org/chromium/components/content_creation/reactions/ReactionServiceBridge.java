// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.reactions;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.util.List;

/**
 * Bridge class in charge of forwarding requests to the native counterpart of
 * the ReactionService bridge.
 */
@JNINamespace("content_creation")
public class ReactionServiceBridge implements ReactionService {
    private long mNativeReactionServiceBridge;

    private ReactionServiceBridge(long nativePtr) {
        mNativeReactionServiceBridge = nativePtr;
    }

    @CalledByNative
    private static ReactionServiceBridge create(long nativePtr) {
        return new ReactionServiceBridge(nativePtr);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeReactionServiceBridge = 0;
    }

    @Override
    public void getReactions(Callback<List<ReactionMetadata>> callback) {
        if (mNativeReactionServiceBridge == 0) return;
        ReactionServiceBridgeJni.get().getReactions(mNativeReactionServiceBridge, this, callback);
    }

    @NativeMethods
    interface Natives {
        void getReactions(long nativeReactionServiceBridge, ReactionServiceBridge caller,
                Callback<List<ReactionMetadata>> callback);
    }
}