// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.TruthJUnit.assume;

import android.os.Build;
import android.os.Process;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;

/** Tests wrapper specific features. */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public class AndroidHttpEngineWrapperTest {
    @Before
    public void setUp() {
        assume().that(Build.VERSION.SDK_INT).isAtLeast(Build.VERSION_CODES.UPSIDE_DOWN_CAKE);
    }

    @Test
    public void testSetThreadPriorityMethod_runsOnce() {
        AndroidHttpEngineWrapper engine =
                new AndroidHttpEngineWrapper(null, Process.THREAD_PRIORITY_FOREGROUND);

        assertThat(engine.setThreadPriority()).isTrue();
        assertThat(engine.setThreadPriority()).isFalse();
    }
}
