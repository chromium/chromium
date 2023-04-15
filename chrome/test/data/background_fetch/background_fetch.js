// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Background Fetch Id to use when its value is not significant.
const kBackgroundFetchId = 'bg-fetch-id';
const kBackgroundFetchResource = [ '/background_fetch/types_of_cheese.txt' ];
const kIcon = [
  {
    src: '/notifications/icon.png',
    sizes: '100x100',
    type: 'image/png',
  }
];

function RegisterServiceWorker() {
  return navigator.serviceWorker.register('sw.js').then(() => {
    return 'ok - service worker registered';
  }).catch(formatError);
}

// Starts a Background Fetch request for a single to-be-downloaded file.
function StartSingleFileDownload() {
  return navigator.serviceWorker.ready.then(swRegistration => {
    const options = {
      icons: kIcon,
      title: 'Single-file Background Fetch'
    };

    return swRegistration.backgroundFetch.fetch(
        kBackgroundFetchId, kBackgroundFetchResource, options);
  }).then(bgFetchRegistration => {
    return 'ok';
  }).catch(formatError);
}

// Starts a Background Fetch with multiple files.
function StartFetchWithMultipleFiles() {
  return navigator.serviceWorker.ready.then(swRegistration => {
    const options = {
      icons: kIcon,
      title: 'multi-file Background Fetch',
    };

    const requests = Array(100)
        .fill('/background_fetch/types_of_cheese.txt')
        .map((req, idx) => `${req}?idx=${idx}`);

    return swRegistration.backgroundFetch.fetch(
        kBackgroundFetchId, requests, options);
  }).then(bgFetchRegistration => {
    return 'ok';
  }).catch(formatError);
}

// Starts a Background Fetch request for a single to-be-downloaded file, with
// downloadTotal greater than the actual size.
function StartSingleFileDownloadWithBiggerThanActualDownloadTotal() {
  return navigator.serviceWorker.ready.then(swRegistration => {
    const options = {
      icons: kIcon,
      title: 'Single-file Background Fetch with downloadTotal too high',
      downloadTotal: 1000,
    };

    return swRegistration.backgroundFetch.fetch(
        kBackgroundFetchId, kBackgroundFetchResource, options);
  }).then(bgFetchRegistration => {
    return 'ok';
  }).catch(formatError);
}

// Starts a Background Fetch request for a single to-be-downloaded file, with
// downloadTotal smaller than the actual size.
function StartSingleFileDownloadWithSmallerThanActualDownloadTotal() {
  return navigator.serviceWorker.ready.then(swRegistration => {
    const options = {
      icons: kIcon,
      title: 'Single-file Background Fetch with downloadTotal too low',
      downloadTotal: 80,
    };

    return swRegistration.backgroundFetch.fetch(
        kBackgroundFetchId, kBackgroundFetchResource, options);
  }).then(bgFetchRegistration => {
    return 'ok';
  }).catch(formatError);
}

// Starts a Background Fetch request for a single to-be-downloaded file, with
// downloadTotal equal to the actual size (in bytes).
function StartSingleFileDownloadWithCorrectDownloadTotal() {
  return navigator.serviceWorker.ready.then(swRegistration => {
    const options = {
      icons: kIcon,
      title: 'Single-file Background Fetch with accurate downloadTotal',
      downloadTotal: 82,
    };

    return swRegistration.backgroundFetch.fetch(
        kBackgroundFetchId, kBackgroundFetchResource, options);
  }).then(bgFetchRegistration => {
    return 'ok';
  }).catch(formatError);
}

// Returns the event's payload, if the payload is recognized. Otherwise returns
// an error string.
function filterMessage(event) {
  const expectedResponses = [
    'backgroundfetchsuccess',
    'backgroundfetchfail',
    'backgroundfetchabort',
    'permissionerror',
    'ok',
  ];
  if (expectedResponses.includes(event.data))
    return event.data;
  return Error('Unexpected message received: ' + event.data).toString();
}

// Starts a Backgound Fetch that should succeed.
function RunFetchTillCompletion() {
  const resources = [
    '/background_fetch/types_of_cheese.txt?idx=1',
    '/background_fetch/types_of_cheese.txt?idx=2',
  ];
  return navigator.serviceWorker.ready.then(swRegistration => {
    const onMessagePromise = new Promise(resolve => {
      navigator.serviceWorker.addEventListener('message', resolve);
    });
    return swRegistration.backgroundFetch.fetch(
        kBackgroundFetchId, resources)
        .then(() => onMessagePromise)
        .then(filterMessage);
  }).catch(formatError);
}

// Starts a Background Fetch that should fail due to a missing resource.
function RunFetchTillCompletionWithMissingResource() {
  const resources = [
    '/background_fetch/types_of_cheese.txt',
    '/background_fetch/missing_cat.txt',
  ];
  return navigator.serviceWorker.ready.then(swRegistration => {
    const onMessagePromise = new Promise(resolve => {
      navigator.serviceWorker.addEventListener('message', resolve);
    });
    return swRegistration.backgroundFetch.fetch(
        kBackgroundFetchId, resources)
        .then(() => onMessagePromise)
        .then(filterMessage);
  }).catch(formatError);
}

// Starts a Background Fetch that should fail due to a missing resource.
function RunFetchAnExpectAnException() {
  const resources = [
    '/background_fetch/types_of_cheese.txt',
    '/background_fetch/missing_cat.txt',
  ];
  return navigator.serviceWorker.ready.then(swRegistration => {
    return swRegistration.backgroundFetch.fetch(kBackgroundFetchId, resources);
  }).then(formatError)
    .catch(e => e.message);
}

// Starts a Background Fetch with an upload that should succeed.
function RunFetchTillCompletionWithUpload() {
  const request = new Request('/background_fetch/upload',
                              {method: 'POST', body: 'upload!'});
  return navigator.serviceWorker.ready.then(swRegistration => {
    const onMessagePromise = new Promise(resolve => {
      navigator.serviceWorker.addEventListener('message', resolve);
    });
    return swRegistration.backgroundFetch.fetch(
        kBackgroundFetchId, request)
        .then(() => onMessagePromise)
        .then(filterMessage);
  }).catch(formatError);
}

function StartFetchFromServiceWorker() {
  const onMessagePromise = new Promise(resolve => {
    navigator.serviceWorker.addEventListener('message', resolve);
  });
  return navigator.serviceWorker.ready
    .then(reg => reg.active.postMessage('fetch'))
    .then(() => onMessagePromise)
    .then(filterMessage);
}

function StartFetchFromServiceWorkerNoWait() {
  navigator.serviceWorker.ready.then(
    reg => reg.active.postMessage('fetchnowait'));
}

function StartFetchFromIframe() {
  const iframe = document.createElement('iframe');
  return new Promise(resolve => {
    window.addEventListener('message', resolve);
    iframe.src = '/background_fetch/background_fetch_iframe.html';
    document.body.appendChild(iframe);
  }).then(filterMessage);
}

function StartFetchFromIframeNoWait() {
  const iframe = document.createElement('iframe');
  return new Promise(resolve => {
    window.addEventListener('message', resolve);
    iframe.src = '/background_fetch/background_fetch_iframe_nowait.html';
    document.body.appendChild(iframe);
  }).then(filterMessage);
}
