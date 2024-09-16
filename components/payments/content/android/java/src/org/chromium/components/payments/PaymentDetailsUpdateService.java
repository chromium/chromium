// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

/**
 * A bound service responsible for receiving change payment method, shipping option, and shipping
 * address calls from an inoked native payment app.
 */
public class PaymentDetailsUpdateService extends Service {
    // AIDL calls can happen on multiple threads in parallel. The binder uses PostTask for
    // synchronization since locks are discouraged in Chromium. The UI thread task runner is used
    // rather than a SequencedTaskRunner since the state of the helper class is also changed by
    // ChromePaymentRequestService.java, which runs on the UI thread.
    private final IPaymentDetailsUpdateService.Stub mBinder =
            new IPaymentDetailsUpdateService.Stub() {
                @Override
                public void changePaymentMethod(
                        Bundle paymentHandlerMethodData,
                        IPaymentDetailsUpdateServiceCallback callback) {
                    int callingUid = Binder.getCallingUid();
                    PostTask.runOrPostTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                if (!PaymentDetailsUpdateServiceHelper.getInstance()
                                        .isCallerAuthorized(callingUid)) {
                                    return;
                                }
                                PaymentDetailsUpdateServiceHelper.getInstance()
                                        .changePaymentMethod(paymentHandlerMethodData, callback);
                            });
                }

                @Override
                public void changeShippingOption(
                        String shippingOptionId, IPaymentDetailsUpdateServiceCallback callback) {
                    int callingUid = Binder.getCallingUid();
                    PostTask.runOrPostTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                if (!PaymentDetailsUpdateServiceHelper.getInstance()
                                        .isCallerAuthorized(callingUid)) {
                                    return;
                                }
                                PaymentDetailsUpdateServiceHelper.getInstance()
                                        .changeShippingOption(shippingOptionId, callback);
                            });
                }

                @Override
                public void changeShippingAddress(
                        Bundle shippingAddress, IPaymentDetailsUpdateServiceCallback callback) {
                    int callingUid = Binder.getCallingUid();
                    PostTask.runOrPostTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                if (!PaymentDetailsUpdateServiceHelper.getInstance()
                                        .isCallerAuthorized(callingUid)) {
                                    return;
                                }
                                PaymentDetailsUpdateServiceHelper.getInstance()
                                        .changeShippingAddress(shippingAddress, callback);
                            });
                }
            };

    @Override
    public IBinder onBind(Intent intent) {
        RecordHistogram.recordBooleanHistogram(
                "PaymentRequest.PaymentDetailsUpdateService.Bind", true);
        return mBinder;
    }
}
