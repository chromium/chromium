// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var activatedServiceWorkerPromise = null;
var messagePort = null;

// Returns a promise that will be resolved with an activated Service
// Worker, or rejects when the Service Worker could not be started. There
// will be a message port to and from the worker in |messagePort|.
function GetActivatedServiceWorker(script, scope) {
  if (activatedServiceWorkerPromise == null) {
    activatedServiceWorkerPromise =
        navigator.serviceWorker.getRegistration(scope)
        .then(function(registration) {
          // Unregister any existing Service Worker.
          if (registration)
            return registration.unregister();
        }).then(function() {
          // Register the Service Worker again.
          return navigator.serviceWorker.register(script, { scope: scope });
        }).then(function(registration) {
          if (registration.active) {
            return registration;
          } else if (registration.waiting || registration.installing) {
            var worker = registration.waiting || registration.installing;
            return new Promise(function(resolve) {
              worker.addEventListener('statechange', function () {
                if (worker.state === 'activated')
                  resolve(registration);
              });
            });
          } else {
            return Promise.reject('Service Worker in invalid state.');
          }
        }).then(function(registration) {
          return new Promise(function(resolve) {
            var channel = new MessageChannel();
            channel.port1.addEventListener('message', function(event) {
              if (event.data == 'ready')
                resolve(registration);
            });

            registration.active.postMessage(channel.port2,
                                            [ channel.port2 ]);

            messagePort = channel.port1;
            messagePort.start();
          });
        });
  }

  return activatedServiceWorkerPromise;
}
