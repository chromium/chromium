// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function RegisterServiceWorker() {
  navigator.serviceWorker.register('background_sync_service_worker.js')
    .then(() => {
      sendResultToTest('ok - service worker registered');
    }).catch(sendErrorToTest);
}

function hasTag(tag) {
  navigator.serviceWorker.ready
    .then(swRegistration => swRegistration.sync.getTags())
    .then(tags => {
      if (tags.indexOf(tag) >= 0) {
        sendResultToTest('ok - ' + tag + ' found');
      } else {
        sendResultToTest('error - ' + tag + ' not found');
      }
    })
    .catch(sendErrorToTest);
}

window.addEventListener('beforeunload', event => {
  navigator.serviceWorker.ready.then(async swRegistration => {
    return await swRegistration.sync.register('test');
  });
});
