// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

/** Interface for factories that create payment apps. */
public interface PaymentAppFactoryInterface {
    /**
     * Creates payment apps for the |delegate|. When this method is invoked, each factory must:
     * 1) Call delegate.onCanMakePaymentCalculated(canMakePayment) exactly once.
     * 2) Filter available apps based on delegate.getMethodData().
     * 3) Call delegate.onPaymentAppCreated(app) for apps that match the method data.
     * 4) Call delegate.onDoneCreatingPaymentApps(this) exactly once.
     *
     * If called while the RenderFrameHost object is still available in Java, but its counterparts
     * has been deleted in C++, then none of the `delegate` methods are expected to be called,
     * because the frame is being unloaded.
     *
     * @param delegate Provides information about payment request and receives a list of payment
     * apps.
     */
    void create(PaymentAppFactoryDelegate delegate);
}
