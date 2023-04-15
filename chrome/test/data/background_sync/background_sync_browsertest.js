// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function RegisterServiceWorker() {
  return navigator.serviceWorker.register('background_sync_service_worker.js')
    .then(() => {
      return 'ok - service worker registered';
    }).catch(formatError);
}

function hasTag(tag) {
  return navigator.serviceWorker.ready
    .then(swRegistration => swRegistration.sync.getTags())
    .then(tags => {
      if (tags.indexOf(tag) >= 0) {
        return 'ok - ' + tag + ' found';
      } else {
        return 'error - ' + tag + ' not found';
      }
    })
    .catch(formatError);
}

window.addEventListener('beforeunload', event => {
  navigator.serviceWorker.ready.then(async swRegistration => {
    return await swRegistration.sync.register('test');
  });
});
