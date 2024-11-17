// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import static org.mockito.AdditionalAnswers.answerVoid;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RobolectricTestRunner;

import org.chromium.base.test.util.Feature;

/** Tests for PaymentDetailsUpdateConnection. */
@RunWith(RobolectricTestRunner.class)
public class PaymentDetailsUpdateConnectionTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private IBinder mPaymentAppServiceBinder;
    @Mock private IPaymentDetailsUpdateServiceCallback mPaymentAppService;
    @Mock private IPaymentDetailsUpdateService.Stub mBrowserService;
    @Mock private Context mContext;

    @Test
    @Feature({"Payments"})
    public void testConnectionBindsToService() throws Throwable {
        Intent intent = new Intent();
        PaymentDetailsUpdateConnection connection =
                new PaymentDetailsUpdateConnection(mContext, intent, mBrowserService);

        connection.connectToService();

        verify(mContext).bindService(eq(intent), eq(connection), eq(Context.BIND_AUTO_CREATE));
    }

    @Test
    @Feature({"Payments"})
    public void testConnectionSetsUpTheService() throws Throwable {
        Intent intent = new Intent();
        PaymentDetailsUpdateConnection connection =
                new PaymentDetailsUpdateConnection(mContext, intent, mBrowserService);
        doReturn(mPaymentAppService).when(mPaymentAppServiceBinder).queryLocalInterface(any());
        doAnswer((invocation) -> {
            ((ServiceConnection) invocation.getArgument(1))
                    .onServiceConnected(
                            /* name= */ null,
                            /* service= */ mPaymentAppServiceBinder);
            return true;
        })
                .when(mContext)
                .bindService(eq(intent), eq(connection), eq(Context.BIND_AUTO_CREATE));

        connection.connectToService();

        verify(mPaymentAppService).setPaymentDetailsUpdateService(eq(mBrowserService));
    }

    @Test
    @Feature({"Payments"})
    public void testNoPermissionToConnectUnbindsService() throws Throwable {
        Intent intent = new Intent();
        PaymentDetailsUpdateConnection connection =
                new PaymentDetailsUpdateConnection(mContext, intent, mBrowserService);
        doAnswer(answerVoid((invocation) -> { throw new SecurityException("Permission denied"); }))
                .when(mContext)
                .bindService(eq(intent), eq(connection), eq(Context.BIND_AUTO_CREATE));

        connection.connectToService();

        verify(mContext).unbindService(eq(connection));
    }

    @Test
    @Feature({"Payments"})
    public void testNullServiceUnbindsService() throws Throwable {
        Intent intent = new Intent();
        PaymentDetailsUpdateConnection connection =
                new PaymentDetailsUpdateConnection(mContext, intent, mBrowserService);
        doAnswer((invocation) -> {
            ((ServiceConnection) invocation.getArgument(1))
                    .onServiceConnected(/* name= */ null, /* service= */ null);
            return true;
        })
                .when(mContext)
                .bindService(eq(intent), eq(connection), eq(Context.BIND_AUTO_CREATE));

        connection.connectToService();

        verify(mContext).unbindService(eq(connection));
    }

    @Test
    @Feature({"Payments"})
    public void testErrorsInPaymentAppsServiceUnbindsService() throws Throwable {
        Intent intent = new Intent();
        PaymentDetailsUpdateConnection connection =
                new PaymentDetailsUpdateConnection(mContext, intent, mBrowserService);
        doReturn(mPaymentAppService).when(mPaymentAppServiceBinder).queryLocalInterface(any());
        doAnswer((invocation) -> {
            ((ServiceConnection) invocation.getArgument(1))
                    .onServiceConnected(
                            /* name= */ null,
                            /* service= */ mPaymentAppServiceBinder);
            return true;
        })
                .when(mContext)
                .bindService(eq(intent), eq(connection), eq(Context.BIND_AUTO_CREATE));
        doAnswer((invocation) -> { throw new Exception("Internal error"); })
                .when(mPaymentAppService)
                .setPaymentDetailsUpdateService(eq(mBrowserService));

        connection.connectToService();

        verify(mContext).unbindService(eq(connection));
    }

    @Test
    @Feature({"Payments"})
    public void testServiceDisconnectUnbindsService() throws Throwable {
        PaymentDetailsUpdateConnection connection =
                new PaymentDetailsUpdateConnection(mContext, new Intent(), mBrowserService);
        connection.connectToService();

        connection.onServiceDisconnected(/*name=*/null);

        verify(mContext).unbindService(eq(connection));
    }

    @Test
    @Feature({"Payments"})
    public void testNullBindingUnbindsService() throws Throwable {
        PaymentDetailsUpdateConnection connection =
                new PaymentDetailsUpdateConnection(mContext, new Intent(), mBrowserService);
        connection.connectToService();

        connection.onNullBinding(/*name=*/null);

        verify(mContext).unbindService(eq(connection));
    }

    @Test
    @Feature({"Payments"})
    public void testDeadBindingUnbindsService() throws Throwable {
        PaymentDetailsUpdateConnection connection =
                new PaymentDetailsUpdateConnection(mContext, new Intent(), mBrowserService);
        connection.connectToService();

        connection.onBindingDied(/*name=*/null);

        verify(mContext).unbindService(eq(connection));
    }

    @Test
    @Feature({"Payments"})
    public void testTerminateConnectionUnbindsService() throws Throwable {
        PaymentDetailsUpdateConnection connection =
                new PaymentDetailsUpdateConnection(mContext, new Intent(), mBrowserService);
        connection.connectToService();

        connection.terminateConnection();

        verify(mContext).unbindService(eq(connection));
    }

    @Test
    @Feature({"Payments"})
    public void testTerminateConnectionNoOpWhenNotConnected() {
        PaymentDetailsUpdateConnection connection =
                new PaymentDetailsUpdateConnection(mContext, new Intent(), mBrowserService);

        connection.terminateConnection();

        verify(mContext, never()).unbindService(any());
    }
}
