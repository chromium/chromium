// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Background Fetch Id to use when its value is not significant.
const kBackgroundFetchId = 'bg-fetch-id';
const kBackgroundFetchResource = [ '/background_fetch/types_of_cheese.txt' ];
const kIcon = [
  {
    src: '/notifications/icon.png',
    sizes: '100x100',
    type: 'image/png'
  }
];

function RegisterServiceWorker() {
  navigator.serviceWorker.register('sw.js').then(() => {
    sendResultToTest('ok - service worker registered');
  }).catch(sendErrorToTest);
}

// Starts a Background Fetch request for a single to-be-downloaded file.
function StartSingleFileDownload() {
  navigator.serviceWorker.ready.then(swRegistration => {
    const options = {
      icons: kIcon,
      title: 'Single-file Background Fetch'
    };

    return swRegistration.backgroundFetch.fetch(
        kBackgroundFetchId, kBackgroundFetchResource, options);
  }).then(bgFetchRegistration => {
    sendResultToTest('ok');
  }).catch(sendErrorToTest);
}

// Starts a Background Fetch request for a single to-be-downloaded file, with
// downloadTotal greater than the actual size.
function StartSingleFileDownloadWithBiggerThanActualDownloadTotal() {
  navigator.serviceWorker.ready.then(swRegistration => {
    const options = {
      icons: kIcon,
      title: 'Single-file Background Fetch with downloadTotal too high',
      downloadTotal: 1000,
    };

    return swRegistration.backgroundFetch.fetch(
        kBackgroundFetchId, kBackgroundFetchResource, options);
  }).then(bgFetchRegistration => {
    sendResultToTest('ok');
  }).catch(sendErrorToTest);
}

// Starts a Background Fetch request for a single to-be-downloaded file, with
// downloadTotal smaller than the actual size.
function StartSingleFileDownloadWithSmallerThanActualDownloadTotal() {
  navigator.serviceWorker.ready.then(swRegistration => {
    const options = {
      icons: kIcon,
      title: 'Single-file Background Fetch with downloadTotal too low',
      downloadTotal: 80,
    };

    return swRegistration.backgroundFetch.fetch(
        kBackgroundFetchId, kBackgroundFetchResource, options);
  }).then(bgFetchRegistration => {
    sendResultToTest('ok');
  }).catch(sendErrorToTest);
}

// Starts a Background Fetch request for a single to-be-downloaded file, with
// downloadTotal equal to the actual size (in bytes).
function StartSingleFileDownloadWithCorrectDownloadTotal() {
  navigator.serviceWorker.ready.then(swRegistration => {
    const options = {
      icons: kIcon,
      title: 'Single-file Background Fetch with accurate downloadTotal',
      downloadTotal: 82,
    };

    return swRegistration.backgroundFetch.fetch(
        kBackgroundFetchId, kBackgroundFetchResource, options);
  }).then(bgFetchRegistration => {
    sendResultToTest('ok');
  }).catch(sendErrorToTest);
}

// Listens for a postMessage from sw.js and sends the result to the test.
navigator.serviceWorker.addEventListener('message', event => {
  const expectedResponses = [
    'backgroundfetchsuccess',
    'backgroundfetchfail',
    'permissionerror',
    'ok',
  ];
  if (expectedResponses.includes(event.data))
    sendResultToTest(event.data);
  else
    sendErrorToTest(Error('Unexpected message received: ' + event.data));
});

// Starts a Backgound Fetch that should succeed.
function RunFetchTillCompletion() {
  const resources = [
    '/background_fetch/types_of_cheese.txt?idx=1',
    '/background_fetch/types_of_cheese.txt?idx=2',
  ];
  navigator.serviceWorker.ready.then(swRegistration => {
    return swRegistration.backgroundFetch.fetch(
        kBackgroundFetchId, resources);
  }).catch(sendErrorToTest);
}

// Starts a Background Fetch that should fail due to a missing resource.
function RunFetchTillCompletionWithMissingResource() {
  const resources = [
    '/background_fetch/types_of_cheese.txt',
    '/background_fetch/missing_cat.txt',
  ];
  navigator.serviceWorker.ready.then(swRegistration => {
    return swRegistration.backgroundFetch.fetch(
        kBackgroundFetchId, resources);
  }).catch(sendErrorToTest);
}

// Starts a Background Fetch that should fail due to a missing resource.
function RunFetchAnExpectAnException() {
  const resources = [
    '/background_fetch/types_of_cheese.txt',
    '/background_fetch/missing_cat.txt',
  ];
  navigator.serviceWorker.ready.then(swRegistration => {
    return swRegistration.backgroundFetch.fetch(kBackgroundFetchId, resources);
  }).then(sendErrorToTest)
    .catch(e => sendResultToTest(e.message));
}

function StartFetchFromServiceWorker() {
  navigator.serviceWorker.ready.then(reg => reg.active.postMessage('fetch'));
}

function StartFetchFromServiceWorkerNoWait() {
  navigator.serviceWorker.ready.then(
    reg => reg.active.postMessage('fetchnowait'));
}

function StartFetchFromIframe() {
  const iframe = document.createElement('iframe');
  iframe.src = '/background_fetch/background_fetch_iframe.html';
  document.body.appendChild(iframe);
}

function StartFetchFromIframeNoWait() {
  const iframe = document.createElement('iframe');
  iframe.src = '/background_fetch/background_fetch_iframe_nowait.html';
  document.body.appendChild(iframe);
}
