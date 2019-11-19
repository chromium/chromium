// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.heap_profiling;

import org.chromium.base.annotations.MainDex;

/**
 * Provides direct access to heap_profiling_test_shim, which in turn forwards to
 * heap_profiling::TestDriver. Only used for testing.
 */
@MainDex
public class HeapProfilingTestShim {
    public HeapProfilingTestShim() {
        mNativeHeapProfilingTestShim = nativeInit();
    }

    /**
     *  When |dynamicallyStartProfiling| is true, the test harness is
     *  responsible for starting profiling for the relevant processes.
     *  When |pseudoStacks| is true, the stacks use trace-event based stacks
     *  rather than native stacks.
     */
    public boolean runTestForMode(String mode, boolean dynamicallyStartProfiling, String stackMode,
            boolean shouldSample, boolean sampleEverything) {
        return nativeRunTestForMode(mNativeHeapProfilingTestShim, mode, dynamicallyStartProfiling,
                stackMode, shouldSample, sampleEverything);
    }

    /**
     * Clean up the C++ side of this class.
     * After the call, this class instance shouldn't be used.
     */
    public void destroy() {
        if (mNativeHeapProfilingTestShim != 0) {
            nativeDestroy(mNativeHeapProfilingTestShim);
            mNativeHeapProfilingTestShim = 0;
        }
    }

    private long mNativeHeapProfilingTestShim;
    private native long nativeInit();
    private native void nativeDestroy(long nativeHeapProfilingTestShim);
    private native boolean nativeRunTestForMode(long nativeHeapProfilingTestShim, String mode,
            boolean dynamicallyStartProfiling, String stackMode, boolean shouldSample,
            boolean sampleEverything);
}
