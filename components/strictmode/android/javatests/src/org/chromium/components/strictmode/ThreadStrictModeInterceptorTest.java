// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.strictmode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.os.StrictMode;
import android.os.StrictMode.ThreadPolicy;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.strictmode.test.disk_write_helper.DiskWriteHelper;
import org.chromium.components.strictmode.test.disk_write_proxy.DiskWriteProxy;

/** Tests for {@link ThreadStrictModeInterceptor}. */
@Batch(Batch.PER_CLASS)
@RunWith(BaseJUnit4ClassRunner.class)
public class ThreadStrictModeInterceptorTest {
    /** Test that the penalty is not notified about permitted strict mode exceptions. */
    @Test
    @SmallTest
    public void testPermitted() {
        CallbackHelper strictModeDetector = new CallbackHelper();
        ThreadStrictModeInterceptor.Builder threadInterceptor =
                new ThreadStrictModeInterceptor.Builder();
        threadInterceptor.addAllowedMethod(
                Violation.DETECT_DISK_IO,
                "org.chromium.components.strictmode.test.disk_write_helper.DiskWriteHelper#doDiskWrite");
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    installThreadInterceptor(threadInterceptor, strictModeDetector);
                    DiskWriteHelper.doDiskWrite();
                });

        // Wait for any tasks posted to the main thread by android.os.StrictMode to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        assertEquals(0, strictModeDetector.getCallCount());
    }

    /** Test that the penalty is notified about unpermitted strict mode exceptions. */
    @Test
    @SmallTest
    public void testNotPermitted() {
        CallbackHelper strictModeDetector = new CallbackHelper();
        ThreadStrictModeInterceptor.Builder threadInterceptor =
                new ThreadStrictModeInterceptor.Builder();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    installThreadInterceptor(threadInterceptor, strictModeDetector);
                    DiskWriteHelper.doDiskWrite();
                });

        // Wait for any tasks posted to the main thread by android.os.StrictMode to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        assertTrue(strictModeDetector.getCallCount() >= 1);
    }

    /**
     * Test that when ThreadStrictModeInterceptor#onlyDetectViolationsForPackage() is used, that the
     * penalty is not notified when org.chromium.content code calls org.chromium.chrome code and the
     * org.chromium.chrome code causes the strict mode violation. This occurs when
     * org.chromium.chrome code registers an observer and the observer gets called.
     */
    @Test
    @SmallTest
    public void testOnlyDetectViolationsForPackageCalleeBlocklisted() {
        CallbackHelper strictModeDetector = new CallbackHelper();
        ThreadStrictModeInterceptor.Builder threadInterceptor =
                new ThreadStrictModeInterceptor.Builder();
        threadInterceptor.onlyDetectViolationsForPackage(
                "org.chromium.components.strictmode.test.disk_write_proxy",
                "org.chromium.components.strictmode.test.disk_write_helper");
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    installThreadInterceptor(threadInterceptor, strictModeDetector);
                    DiskWriteProxy.callDiskWriteHelper();
                });

        // Wait for any tasks posted to the main thread by android.os.StrictMode to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        assertEquals(0, strictModeDetector.getCallCount());
    }

    /**
     * Test that when ThreadStrictModeInterceptor#onlyDetectViolationsForPackage() is used, that the
     * penalty is notified when org.chromium.chrome code calls org.chromium.content code and the
     * org.chromium.content code causes the strict mode violation.
     */
    @Test
    @SmallTest
    public void testOnlyDetectViolationsForPackageCallerBlocklisted() {
        CallbackHelper strictModeDetector = new CallbackHelper();
        ThreadStrictModeInterceptor.Builder threadInterceptor =
                new ThreadStrictModeInterceptor.Builder();
        // .test package names are used for onlyDetectViolationsForPackage() to prevent the
        // package names from being obfuscated.
        threadInterceptor.onlyDetectViolationsForPackage(
                "org.chromium.components.strictmode.test.disk_write_helper",
                "org.chromium.components.strictmode.test.disk_write_proxy");
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    installThreadInterceptor(threadInterceptor, strictModeDetector);
                    DiskWriteProxy.callDiskWriteHelper();
                });

        // Wait for any tasks posted to the main thread by android.os.StrictMode to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        assertTrue(strictModeDetector.getCallCount() >= 1);
    }

    /**
     * Test that when ThreadStrictModeInterceptor#onlyDetectViolationsForPackage() is used, that the
     * penalty is not notified when the strict mode violation occurs in an unrelated package.
     */
    @Test
    @SmallTest
    public void testOnlyDetectViolationForPackageViolationInUnrelatedPackage() {
        CallbackHelper strictModeDetector = new CallbackHelper();
        ThreadStrictModeInterceptor.Builder threadInterceptor =
                new ThreadStrictModeInterceptor.Builder();
        threadInterceptor.onlyDetectViolationsForPackage(
                "org.chromium.components.strictmode.test.non_existent",
                "org.chromium.components.strictmode.test.non_existent2");
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    installThreadInterceptor(threadInterceptor, strictModeDetector);
                    DiskWriteProxy.callDiskWriteHelper();
                });

        // Wait for any tasks posted to the main thread by android.os.StrictMode to complete.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        assertEquals(0, strictModeDetector.getCallCount());
    }

    private void installThreadInterceptor(
            ThreadStrictModeInterceptor.Builder threadInterceptor,
            CallbackHelper strictModeDetector) {
        ThreadPolicy.Builder threadPolicy =
                new ThreadPolicy.Builder(StrictMode.getThreadPolicy()).penaltyLog().detectAll();
        threadInterceptor.setCustomPenalty(
                violation -> {
                    strictModeDetector.notifyCalled();
                });
        threadInterceptor.build().install(threadPolicy.build());
    }
}
