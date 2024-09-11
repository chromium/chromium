// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth.test;

import static org.junit.Assume.assumeTrue;

import android.os.Build;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

/**
 * Tests for IpProtectionAuthClient and associated classes.
 *
 * <p>These tests mostly call into native code (ip_protection_auth_test_natives.cc) and interact
 * with "mock" services hosted in a secondary APK.
 *
 * <p>The usage of native test code for Java-hosted tests along with using native functionality like
 * RunLoop and CHECK has the potential to make any test failures more confusing, including native
 * crashes rather than Java AssertionErrors and global task state contamination across unrelated
 * test suites. As such, these tests are batched PER_CLASS to isolate such failures.
 */
@MediumTest
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public final class IpProtectionAuthTest {
    @Before
    public void setUp() throws Exception {
        LibraryLoader.getInstance().ensureInitialized();
    }

    @Test
    public void nativeCreateConnectedInstanceTest() throws Exception {
        IpProtectionAuthTestNatives.createConnectedInstanceForTesting();
    }

    @Test
    public void nativeNonexistantServiceTest() throws Exception {
        IpProtectionAuthTestNatives.testNonexistantService();
    }

    @Test
    public void nativeNullBindingServiceTest() throws Exception {
        // API levels < 28 (Pie) do not support null bindings
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.P);
        IpProtectionAuthTestNatives.testNullBindingService();
    }

    @Test
    public void nativeDisabledServiceTest() throws Exception {
        IpProtectionAuthTestNatives.testDisabledService();
    }

    @Test
    public void nativeRestrictedServiceTest() throws Exception {
        IpProtectionAuthTestNatives.testRestrictedService();
    }

    @Test
    public void nativeGetInitialDataTest() throws Exception {
        IpProtectionAuthTestNatives.testGetInitialData();
    }

    @Test
    public void nativeAuthAndSignTest() throws Exception {
        IpProtectionAuthTestNatives.testAuthAndSign();
    }

    @Test
    public void nativeGetProxyConfigTest() throws Exception {
        IpProtectionAuthTestNatives.testGetProxyConfig();
    }

    @Test
    public void nativeTransientErrorTest() throws Exception {
        IpProtectionAuthTestNatives.testTransientError();
    }

    @Test
    public void nativePersistentErrorTest() throws Exception {
        IpProtectionAuthTestNatives.testPersistentError();
    }

    @Test
    public void nativeIllegalErrorCodeTest() throws Exception {
        IpProtectionAuthTestNatives.testIllegalErrorCode();
    }

    @Test
    public void nativeNullResponseTest() throws Exception {
        IpProtectionAuthTestNatives.testNullResponse();
    }

    @Test
    public void nativeUnparsableResponseTest() throws Exception {
        IpProtectionAuthTestNatives.testUnparsableResponse();
    }

    @Test
    public void nativeSynchronousErrorTest() throws Exception {
        IpProtectionAuthTestNatives.testSynchronousError();
    }

    @Test
    public void nativeUnresolvedWhenClosedTest() throws Exception {
        IpProtectionAuthTestNatives.testUnresolvedWhenClosed();
    }

    @Test
    public void nativeCrashOnRequestSyncWithoutResponse() throws Exception {
        IpProtectionAuthTestNatives.testCrashOnRequestSyncWithoutResponse();
    }

    @Test
    public void nativeCrashOnRequestAsyncWithoutResponse() throws Exception {
        IpProtectionAuthTestNatives.testCrashOnRequestAsyncWithoutResponse();
    }

    @Test
    public void nativeCrashOnRequestSyncWithResponse() throws Exception {
        IpProtectionAuthTestNatives.testCrashOnRequestSyncWithResponse();
    }

    @Test
    public void nativeUnresolvedCallbacksRejectedAfterCrash() throws Exception {
        IpProtectionAuthTestNatives.testUnresolvedCallbacksRejectedAfterCrash();
    }
}
