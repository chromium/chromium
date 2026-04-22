// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let expectUpdate = false;

const registerServiceWorkerPromise = new Promise(function(resolve, reject) {
  expectUpdate = window.location.hash == '#expect_update';
  let serviceWorkerRegistration;
  navigator.serviceWorker.register('sw.js')
      .then(function() {
        return navigator.serviceWorker.ready;
      })
      .then(function(registration) {
        serviceWorkerRegistration = registration;

        const promises = [registration.update()];
        if (expectUpdate) {
          promises.push(new Promise(function(resolve) {
            registration.onupdatefound = function(e) {
              resolve();
            };
          }));
        }

        return Promise.all(promises);
      })
      .then(function() {
        const installingWorker = serviceWorkerRegistration.installing;
        if (installingWorker) {
          // If there's an installing worker, wait for waiting worker to exist
          // first.
          installingWorker.onstatechange = function(e) {
            if (installingWorker.state == 'installed') {
              chrome.test.assertTrue(!!serviceWorkerRegistration.waiting);
              resolve(serviceWorkerRegistration.active);
            }
          };
        } else {
          chrome.test.assertFalse(expectUpdate);
          chrome.test.assertEq(null, serviceWorkerRegistration.waiting);
          resolve(serviceWorkerRegistration.active);
        }
      })
      .catch(function(err) {
        reject(err);
      });
});

registerServiceWorkerPromise
    .then(function(serviceWorker) {
      const channel = new MessageChannel();
      channel.port1.onmessage = function(e) {
        chrome.test.log(`Message received from SW: ${e.data}`);
        let response = e.data;
        if (expectUpdate) {
          response += ' (with update)';
        }
        chrome.test.sendMessage(response);
      };
      serviceWorker.postMessage('ping', [channel.port2]);
    })
    .catch(function(err) {
      chrome.test.log(err);
      chrome.test.sendMessage('FAILURE');
    });
