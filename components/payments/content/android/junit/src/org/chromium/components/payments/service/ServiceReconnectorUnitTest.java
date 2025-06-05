// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.service;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;

import android.os.Handler;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Tests for the service reconnector behavior. */
@RunWith(BaseRobolectricTestRunner.class)
public class ServiceReconnectorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Reconnectable mConnection;
    @Mock private Handler mHandler;

    @Test
    @Feature({"Payments"})
    public void testTerminateConnectionWhenMaxReconnectsIsZero() throws Exception {
        ServiceReconnector reconnector =
                new ServiceReconnector(mConnection, /* maxRetryNumber= */ 0, mHandler);

        reconnector.onUnexpectedServiceDisconnect();

        Mockito.verify(mConnection).terminateConnection();
    }

    @Test
    @Feature({"Payments"})
    public void testUnbindServiceBeforeReconnect() throws Exception {
        ServiceReconnector reconnector =
                new ServiceReconnector(mConnection, /* maxRetryNumber= */ 1, mHandler);

        reconnector.onUnexpectedServiceDisconnect();

        Mockito.verify(mConnection).unbindService();
        // First reconnect delay = 1 second.
        Mockito.verify(mHandler).postDelayed(any(), eq(1000L));
    }

    @Test
    @Feature({"Payments"})
    public void testIntentionalDisconnectPreventsReconnects() throws Exception {
        ServiceReconnector reconnector =
                new ServiceReconnector(mConnection, /* maxRetryNumber= */ 999, mHandler);

        reconnector.onIntentionalServiceDisconnect();

        Mockito.verify(mHandler).removeCallbacksAndMessages(eq(null));

        reconnector.onUnexpectedServiceDisconnect();

        Mockito.verify(mConnection).terminateConnection();
    }

    @Test
    @Feature({"Payments"})
    public void testThreeReconnectAttempts() throws Exception {
        ServiceReconnector reconnector =
                new ServiceReconnector(mConnection, /* maxRetryNumber= */ 3, mHandler);

        reconnector.onUnexpectedServiceDisconnect();

        // First reconnect delay = 1 second.
        Mockito.verify(mHandler).postDelayed(any(), eq(1000L));

        reconnector.onUnexpectedServiceDisconnect();

        // Second reconnect delay = 2 seconds.
        Mockito.verify(mHandler).postDelayed(any(), eq(2000L));

        reconnector.onUnexpectedServiceDisconnect();

        // Third reconnect delay = 4 seconds.
        Mockito.verify(mHandler).postDelayed(any(), eq(4000L));

        reconnector.onUnexpectedServiceDisconnect();

        // Give up reconnecting after 3 attempts.
        Mockito.verify(mConnection).terminateConnection();
    }
}
