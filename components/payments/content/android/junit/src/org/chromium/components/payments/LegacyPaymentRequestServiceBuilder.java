// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.components.payments.test_support.PaymentRequestServiceBuilder;
import org.chromium.payments.mojom.PaymentRequestClient;

/**
 * TODO(crbug.com/1170916): Removed soon. This class is a temporary replacement of
 * PaymentRequestServiceBuilder, used to transition downstream dependencies.
 */
public class LegacyPaymentRequestServiceBuilder extends PaymentRequestServiceBuilder {
    public LegacyPaymentRequestServiceBuilder(Runnable onClosedListener,
            PaymentRequestClient client, PaymentAppService appService,
            BrowserPaymentRequest browserPaymentRequest, JourneyLogger journeyLogger) {
        super(onClosedListener, client, appService, browserPaymentRequest, journeyLogger);
    }
}
