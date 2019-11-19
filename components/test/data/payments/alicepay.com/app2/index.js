/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Prints output.
 * @param {String} src - Where the message is coming from.
 * @param {String} txt - The text to print.
 */
function output(src, txt) {
  // Handle DOMException:
  if (txt.message) {
    txt = txt.message;
  }
  txt = src + ': ' + txt;
  if (!domAutomationController) {
    txt += ' window.domAutomationController not found.';
  } else {
    domAutomationController.send(txt);
  }
  console.log(txt);
}

/**
 * Installs a payment app.
 * @param {String} method - The payment method name that this app supports.
 */
function install(method) { // eslint-disable-line no-unused-vars
  if (!navigator.serviceWorker) {
    output('install()', 'ServiceWorker API not found.');
    return;
  }

  navigator.serviceWorker.getRegistration('app.js')
      .then((registration) => {
        if (registration) {
          output(
              'serviceWorker.getRegistration()',
              'The ServiceWorker is already installed.');
          return;
        }
        navigator.serviceWorker.register('app.js')
            .then(() => {
              return navigator.serviceWorker.ready;
            })
            .then((registration) => {
              if (!registration.paymentManager) {
                output(
                    'serviceWorker.register()',
                    'PaymentManager API not found.');
                return;
              }

              registration.paymentManager.instruments
                  .set('123456', {name: 'Alice Pay', method: method})
                  .then(() => {
                    output(
                        'instruments.set()',
                        'Payment app for "' + method + '" method installed.');
                  })
                  .catch((error) => {
                    output('instruments.set()', error);
                  });
            })
            .catch((error) => {
              output('serviceWorker.register()', error);
            });
      })
      .catch((error) => {
        output('serviceWorker.getRegistration()', error);
      });
}
