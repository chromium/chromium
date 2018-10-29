// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tracing;

import android.os.ConditionVariable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Helper to run code through JNI layer to test JNI unwinding.
 */
@JNINamespace("tracing")
public final class UnwindTestHelper {
    private static final ConditionVariable sBlock = new ConditionVariable();

    @CalledByNative
    public static void blockCurrentThread() {
        sBlock.block();
        sBlock.close();
    }

    @CalledByNative
    public static void unblockAllThreads() {
        sBlock.open();
    }
}
