// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var getTestURL = function() {
  return chrome.runtime.getURL('./data_for_extension');
};

var workerRegisterAndClaimPromise = function() {
  return new Promise(function(resolve, reject) {
    // Register a ServiceWorker and expect it to control subsequent requests.
    navigator.serviceWorker.register('sw.js').then(function(registration) {
      return navigator.serviceWorker.ready;
    }).then(function(registration) {
      var channel = new MessageChannel();
      // Wait for ServiceWorker to claim itself.
      channel.port1.onmessage = function(e) {
        if (e.data == 'clients claimed') {
          resolve(registration);
        } else {
          reject('Claim failure: ' + e.data);
        }
      };
      registration.active.postMessage('claim', [channel.port2]);
    }).catch(function(err) {
      reject(err);
    });
  });
};

var workerControlsPagePromise = function() {
  return new Promise((resolve) => {
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
};

var fetchWithControlledPagePromise = function() {
  return new Promise(function(resolve, reject) {
    fetch(getTestURL()).then(function(response) {
      return response.text();
    }).then(function(text) {
      if (text != 'SW served data') {
        reject('Fetch() result error[2]: ' + text);
      }
      resolve();
    }).catch(function(err) {
      reject(err);
    });
  });
};

var test = function() {
  var serviceWorkerRegistration;
  // First request would not be controlled by ServiceWorker as the worker
  // doesn't exist yet.
  fetch(getTestURL()).then(function(response) {
    return response.text();
  }).then(function(text) {
    if (text != 'original data\n') {
      throw 'Fetch() result error[1]: ' + text;
    }
    return workerRegisterAndClaimPromise();
  }).then(function(registration) {
    serviceWorkerRegistration = registration;
    return workerControlsPagePromise();
  }).then(function() {
    return fetchWithControlledPagePromise();
  }).then(function() {
    return serviceWorkerRegistration.unregister();
  }).then(function() {
    chrome.test.succeed();
  }).catch(function(err) {
    chrome.test.fail(err);
  });
};

chrome.test.runTests([test]);
