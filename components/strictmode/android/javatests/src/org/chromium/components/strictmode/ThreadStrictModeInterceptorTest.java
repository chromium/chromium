// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.strictmode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.os.StrictMode;
import android.os.StrictMode.ThreadPolicy;
import android.support.test.InstrumentationRegistry;

import androidx.core.content.ContextCompat;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.File;
import java.io.FileOutputStream;

/**
 * Tests for {@link ThreadStrictModeInterceptor}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ThreadStrictModeInterceptorTest {
    /**
     * Test that the penalty is not notified about permitted strict mode exceptions.
     */
    @Test
    @SmallTest
    public void testPermitted() {
        CallbackHelper strictModeDetector = new CallbackHelper();
        ThreadStrictModeInterceptor.Builder threadInterceptor =
                new ThreadStrictModeInterceptor.Builder();
        threadInterceptor.addAllowedMethod(Violation.DETECT_DISK_IO,
                "org.chromium.components.strictmode.ThreadStrictModeInterceptorTest#doDiskWrite");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            installThreadInterceptor(threadInterceptor, strictModeDetector);
            doDiskWrite();
        });

        // Wait for any tasks posted to the main thread by android.os.StrictMode to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        assertEquals(0, strictModeDetector.getCallCount());
    }

    /**
     * Test that the penalty is notified about unpermitted strict mode exceptions.
     */
    @Test
    @SmallTest
    public void testNotPermitted() {
        CallbackHelper strictModeDetector = new CallbackHelper();
        ThreadStrictModeInterceptor.Builder threadInterceptor =
                new ThreadStrictModeInterceptor.Builder();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            installThreadInterceptor(threadInterceptor, strictModeDetector);
            doDiskWrite();
        });

        // Wait for any tasks posted to the main thread by android.os.StrictMode to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        assertTrue(strictModeDetector.getCallCount() >= 1);
    }

    private void doDiskWrite() {
        File dataDir = ContextCompat.getDataDir(InstrumentationRegistry.getTargetContext());
        File prefsDir = new File(dataDir, "shared_prefs");
        File outFile = new File(prefsDir, "random.txt");
        try (FileOutputStream out = new FileOutputStream(outFile)) {
            out.write(1);
        } catch (Exception e) {
        }
    }

    private void installThreadInterceptor(ThreadStrictModeInterceptor.Builder threadInterceptor,
            CallbackHelper strictModeDetector) {
        ThreadPolicy.Builder threadPolicy =
                new ThreadPolicy.Builder(StrictMode.getThreadPolicy()).penaltyLog().detectAll();
        threadInterceptor.setCustomPenalty(violation -> { strictModeDetector.notifyCalled(); });
        threadInterceptor.build().install(threadPolicy.build());
    }
}
