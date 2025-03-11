// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

/**
 * The contents of a merchant checkout page for testing the PaymentRequest API.
 *
 * <p>Sample usage:
 *
 * <pre>
 * loadUrlAsync(
 *         TestWebServer.start().setResponse(
 *                 "/checkout",
 *                 new PaymentRequestTestWebPageContents("https://test-1.example/pay",
 *                                              "https://test-2.example/pay").build(true)));
 * </pre>
 */
public class PaymentRequestTestWebPageContents {
    private final String mPaymentMethodName;
    private final String mOtherPaymentMethodName;

    /**
     * Constructs an instance of test checkout page contents.
     *
     * @param paymentMethodName The payment method name to use in PaymentRequest API.
     * @param otherPaymentMethodName An additional payment method name to use in PaymentRequest API,
     *     if need to request multiple payment methods.
     */
    public PaymentRequestTestWebPageContents(
            String paymentMethodName, String otherPaymentMethodName) {
        mPaymentMethodName = paymentMethodName;
        mOtherPaymentMethodName = otherPaymentMethodName;
    }

    /**
     * Builds the test web page contents for exercising the PaymentRequest API.
     *
     * @param multiplePaymentMethods Whether multiple payment methods should be requested in the
     *     PaymentRequest API call.
     */
    public String build(boolean multiplePaymentMethods) {
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
                const firstMethod = '%s';
                const secondMethod = '%s';
                const total = {label: 'Total', amount: {value: '0.01', currency: 'USD'}};
                return secondMethod
                       ? new PaymentRequest([{supportedMethods: firstMethod},
                                             {supportedMethods: secondMethod}], {total})
                       : new PaymentRequest([{supportedMethods: firstMethod}], {total});
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

        return String.format(
                checkoutPageHtmlFormat,
                mPaymentMethodName,
                multiplePaymentMethods ? mOtherPaymentMethodName : "");
    }
}
