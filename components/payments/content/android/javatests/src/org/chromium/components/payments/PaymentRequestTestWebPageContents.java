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
            <button id="checkPaymentRequestDefined">Check defined</button>
            <button id="checkCanMakePayment">Check can make payment</button>
            <button id="checkHasEnrolledInstrument">Check has enrolled instrument</button>
            <button id="launchPaymentApp">Launch payment app</button>
            <button id="retryPayment">Retry payment</button>

            <script>
              function createPaymentRequest() {
                const total = {label: 'Total', amount: {value: '0.01', currency: 'USD'}};
                return new PaymentRequest([%s], {total}, %s);
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

              document.getElementById('checkPaymentRequestDefined')
                  .addEventListener('click', checkPaymentRequestDefined);
              document.getElementById('checkCanMakePayment')
                  .addEventListener('click', checkCanMakePayment);
              document.getElementById('checkHasEnrolledInstrument')
                  .addEventListener('click', checkHasEnrolledInstrument);
              document.getElementById('launchPaymentApp')
                  .addEventListener('click', launchPaymentApp);
              document.getElementById('retryPayment')
                  .addEventListener('click', retryPayment);

              resultListener.postMessage('Page loaded.');
            </script>
            """;

        return String.format(checkoutPageHtmlFormat, supportedMethods, options);
    }
}
