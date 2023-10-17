// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.environment_integrity;

import com.google.common.util.concurrent.ListenableFuture;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.environment_integrity.enums.IntegrityResponse;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Test that asserts that the upstream implementation of {@link IntegrityServiceBridgeDelegateImpl}
 * has a public no-args constructor and fails with {@link IntegrityResponse#API_NOT_AVAILABLE}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class IntegrityServiceBridgeDelegateImplTest {
    @Test
    public void testDelegateImplHasNoArgsConstructor() {
        IntegrityServiceBridgeDelegate delegate = new IntegrityServiceBridgeDelegateImpl();
        Assert.assertNotNull(delegate);
    }

    @Test
    public void testReportsGmsNotAvailable() {
        IntegrityServiceBridgeDelegate delegate = new IntegrityServiceBridgeDelegateImpl();
        Assert.assertFalse(delegate.canUseGms());
    }

    @Test
    public void testDoesNotProvideHandle() {
        IntegrityServiceBridgeDelegate delegate = new IntegrityServiceBridgeDelegateImpl();
        final ListenableFuture<Long> future =
                delegate.createEnvironmentIntegrityHandle(false, 1000);
        try {
            future.get(1, TimeUnit.SECONDS);
            Assert.fail("Requests for handle on default delegate impl should fail.");
        } catch (ExecutionException e) {
            Assert.assertNotNull(e.getCause());
            Assert.assertTrue(e.getCause() instanceof IntegrityException);
            Assert.assertEquals(
                    IntegrityResponse.API_NOT_AVAILABLE,
                    ((IntegrityException) e.getCause()).getErrorCode());
        } catch (InterruptedException | TimeoutException e) {
            Assert.fail(e.getMessage());
        }
    }

    @Test
    public void testDoesNotProvideToken() {
        IntegrityServiceBridgeDelegate delegate = new IntegrityServiceBridgeDelegateImpl();
        final ListenableFuture<byte[]> future =
                delegate.getEnvironmentIntegrityToken(123456789L, new byte[] {}, 1000);
        try {
            future.get(1, TimeUnit.SECONDS);
            Assert.fail("Requests for token on default delegate impl should fail.");
        } catch (ExecutionException e) {
            Assert.assertNotNull(e.getCause());
            Assert.assertTrue(e.getCause() instanceof IntegrityException);
            Assert.assertEquals(
                    IntegrityResponse.API_NOT_AVAILABLE,
                    ((IntegrityException) e.getCause()).getErrorCode());
        } catch (InterruptedException | TimeoutException e) {
            Assert.fail(e.getMessage());
        }
    }
}
