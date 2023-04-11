// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function register() {
  var script = './sw.js';
  var scope = './';
  return navigator.serviceWorker.register(script, {scope: scope})
    .then(function() { return navigator.serviceWorker.ready; })
    .then(function(registration) {
        var channel = new MessageChannel();
        var saw_message = new Promise(function(resolve, reject) {
            channel.port1.onmessage = function (e) {
              if (e.data == 'clients claimed')
                resolve();
              else
                reject(e.data)
            };
          });
        registration.active.postMessage({port: channel.port2}, [channel.port2]);
        return saw_message;
      })
    .then(function() {
      // Wait for service worker to control us.
      return new Promise(function(resolve, reject) {
        if (navigator.serviceWorker.controller) {
          resolve();
          return;
        }
        navigator.serviceWorker.oncontrollerchange = function(e) {
          if (navigator.serviceWorker.controller) {
            resolve();
            return;
          }
        };
      });
    })
    .then(function() { return fetch('./sw_controlled_check'); })
    .then(function(res) { return res.text(); })
    .catch(function(e) { return 'Fail: ' + e; });
}
