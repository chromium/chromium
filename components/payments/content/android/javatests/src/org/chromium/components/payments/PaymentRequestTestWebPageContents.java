// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import java.util.ArrayList;
import java.util.List;
import java.util.stream.Collectors;

/**
 * The contents of a merchant checkout page for testing PaymentRequest API.
 *
 * <p>Sample usage:
 *
 * <pre>
 * loadUrl(TestWebServer.start().setResponse(
 *         "/checkout",
 *         new PaymentRequestTestWebPageContents().addMethod("https://test-1.example/pay")
 *                                                .addMethod("https://test-2.example/pay")
 *                                                .requestShippingAddress()
 *                                                .requestContactInformation()
 *                                                .build()));
 * </pre>
 */
public class PaymentRequestTestWebPageContents {
    public static final String CHECK_PAYMENT_REQUEST_DEFINED_BUTTON_ID =
            "checkPaymentRequestDefined";
    public static final String CHECK_CAN_MAKE_PAYMENT_BUTTON_ID = "checkCanMakePayment";
    public static final String CHECK_HAS_ENROLLED_INSTRUMENT_BUTTON_ID =
            "checkHasEnrolledInstrument";
    public static final String LAUNCH_PAYMENT_APP_BUTTON_ID = "launchPaymentApp";
    public static final String RETRY_PAYMENT_BUTTON_ID = "retryPayment";

    private final List<String> mMethods = new ArrayList<>();
    private boolean mRequestShippingAddress;
    private boolean mRequestContactInformation;

    /**
     * Adds a payment method to the list of payment methods that will be requested in the
     * PaymentRequest API on the test checkout page.
     *
     * @param method The payment method to request in PaymentRequest API. Must be a URL, e.g.,
     *     "https://payments.example/web-pay". The string should not contain any apostrophes,
     *     because it is encoded into JavaScript via String.format().
     * @return A reference to this {@link PaymentRequestTestWebPageContents} instance.
     */
    public PaymentRequestTestWebPageContents addMethod(String method) {
        assert !method.contains("'") : "Payment method name should not contain any apostrophes.";
        mMethods.add(method);
        return this;
    }

    /**
     * Makes the PaymentRequest API call request user's shipping address.
     *
     * @return A reference to this {@link PaymentRequestTestWebPageContents} instance.
     */
    public PaymentRequestTestWebPageContents requestShippingAddress() {
        mRequestShippingAddress = true;
        return this;
    }

    /**
     * Makes the PaymentRequest API call request user's contact information.
     *
     * @return A reference to this {@link PaymentRequestTestWebPageContents} instance.
     */
    public PaymentRequestTestWebPageContents requestContactInformation() {
        mRequestContactInformation = true;
        return this;
    }

    /**
     * Builds the test web page contents for exercising PaymentRequest API.
     *
     * @return The web page contents.
     */
    @SuppressWarnings("InlineFormatString")
    public String build() {
        String supportedMethods =
                mMethods.stream()
                        .map(method -> String.format("{supportedMethods: '%s'}", method))
                        .collect(Collectors.joining(", "));

        String optionsFormat =
                "{requestShipping: %1$b, requestPayerName: %2$b, "
                        + "requestPayerEmail: %2$b, requestPayerPhone: %2$b}";
        String options =
                String.format(optionsFormat, mRequestShippingAddress, mRequestContactInformation);

        String checkoutPageHtmlFormat =
                """
                <!doctype html>
                <button id="%3$s">Check defined</button>
                <button id="%4$s">Check can make payment</button>
                <button id="%5$s">Check has enrolled instrument</button>
                <button id="%6$s">Launch payment app</button>
                <button id="%7$s">Retry payment</button>

                <script>
                  function createPaymentRequest() {
                    const total = {label: 'Total', amount: {value: '0.01', currency: 'USD'}};
                    return new PaymentRequest([%1$s], {total}, %2$s);
                  }

                  function checkPaymentRequestDefined() {
                    if (!window.PaymentRequest) {
                      resultListener.postMessage('PaymentRequest is not defined.');
                    } else {
                      resultListener.postMessage('PaymentRequest is defined.');
                    }
                  }

                  async function checkCanMakePayment() {
                    try {
                      const request = createPaymentRequest();
                      if (await request.canMakePayment()) {
                        resultListener.postMessage('PaymentRequest can make payments.');
                      } else {
                        resultListener.postMessage('PaymentRequest cannot make payments.');
                      }
                    } catch (e) {
                      resultListener.postMessage(e.toString());
                    }
                  }

                  async function checkHasEnrolledInstrument() {
                    try {
                      const request = createPaymentRequest();
                      if (await request.hasEnrolledInstrument()) {
                        resultListener.postMessage('PaymentRequest has enrolled instrument.');
                      } else {
                        resultListener.postMessage('PaymentRequest does not have enrolled instrument.');
                      }
                    } catch (e) {
                      resultListener.postMessage(e.toString());
                    }
                  }

                  async function launchPaymentApp() {
                    try {
                      const request = createPaymentRequest();
                      const response = await request.show();
                      await response.complete('success');
                      resultListener.postMessage(JSON.stringify(response));
                    } catch (e) {
                      resultListener.postMessage(e.toString());
                    }
                  }

                  async function retryPayment() {
                    try {
                      const request = createPaymentRequest();
                      let response = await request.show();
                      response = await response.retry();
                      await response.complete('success');
                      resultListener.postMessage(JSON.stringify(response));
                    } catch (e) {
                      resultListener.postMessage(e.toString());
                    }
                  }

                  document.getElementById('%3$s')
                      .addEventListener('click', checkPaymentRequestDefined);
                  document.getElementById('%4$s')
                      .addEventListener('click', checkCanMakePayment);
                  document.getElementById('%5$s')
                      .addEventListener('click', checkHasEnrolledInstrument);
                  document.getElementById('%6$s')
                      .addEventListener('click', launchPaymentApp);
                  document.getElementById('%7$s')
                      .addEventListener('click', retryPayment);

                  resultListener.postMessage('Page loaded.');
                </script>
                """;

        return String.format(
                checkoutPageHtmlFormat,
                supportedMethods,
                options,
                CHECK_PAYMENT_REQUEST_DEFINED_BUTTON_ID,
                CHECK_CAN_MAKE_PAYMENT_BUTTON_ID,
                CHECK_HAS_ENROLLED_INSTRUMENT_BUTTON_ID,
                LAUNCH_PAYMENT_APP_BUTTON_ID,
                RETRY_PAYMENT_BUTTON_ID);
    }
}
