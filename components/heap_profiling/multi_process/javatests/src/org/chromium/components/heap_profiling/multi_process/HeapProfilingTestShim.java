// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.heap_profiling.multi_process;

import org.jni_zero.NativeMethods;

/**
 * Provides direct access to heap_profiling_test_shim, which in turn forwards to
 * heap_profiling::TestDriver. Only used for testing.
 */
public class HeapProfilingTestShim {
    public HeapProfilingTestShim() {
        mNativeHeapProfilingTestShim = HeapProfilingTestShimJni.get().init(this);
    }

    /**
     *  When |dynamicallyStartProfiling| is true, the test harness is
     *  responsible for starting profiling for the relevant processes.
     *  When |pseudoStacks| is true, the stacks use trace-event based stacks
     *  rather than native stacks.
     */
    public boolean runTestForMode(
            String mode,
            boolean dynamicallyStartProfiling,
            String stackMode,
            boolean shouldSample,
            boolean sampleEverything) {
        return HeapProfilingTestShimJni.get()
                .runTestForMode(
                        mNativeHeapProfilingTestShim,
                        mode,
                        dynamicallyStartProfiling,
                        stackMode,
                        shouldSample,
                        sampleEverything);
    }

    /**
     * Clean up the C++ side of this class.
     * After the call, this class instance shouldn't be used.
     */
    public void destroy() {
        if (mNativeHeapProfilingTestShim != 0) {
            HeapProfilingTestShimJni.get().destroy(mNativeHeapProfilingTestShim);
            mNativeHeapProfilingTestShim = 0;
        }
    }

    private long mNativeHeapProfilingTestShim;

    @NativeMethods
    interface Natives {
        long init(HeapProfilingTestShim obj);

        void destroy(long nativeHeapProfilingTestShim);

        boolean runTestForMode(
                long nativeHeapProfilingTestShim,
                String mode,
                boolean dynamicallyStartProfiling,
                String stackMode,
                boolean shouldSample,
                boolean sampleEverything);
    }
}
