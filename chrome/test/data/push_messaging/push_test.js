// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// The ResultQueue is a mechanism for passing messages back to the test
// framework.
var resultQueue = new ResultQueue();

// Waits for the given ServiceWorkerRegistration to become ready.
// Shim for https://github.com/w3c/ServiceWorker/issues/770.
function swRegistrationReady(reg) {
  return new Promise((resolve, reject) => {
    if (reg.active) {
      resolve();
      return;
    }

    if (!reg.installing && !reg.waiting) {
      reject(Error('Install failed'));
      return;
    }

    (reg.installing || reg.waiting).addEventListener('statechange', function() {
      if (this.state == 'redundant') {
        reject(Error('Install failed'));
      } else if (this.state == 'activated') {
        resolve();
      }
    });
  });
}

// Notification permission has been coalesced with Push permission. After
// this is granted, Push API subscription can succeed.
function requestNotificationPermission() {
  return new Promise(resolve => {
    Notification.requestPermission(resolve);
  }).then((permission) => 'permission status - ' + permission);
}

function registerServiceWorker() {
  // The base dir used to resolve service_worker.js and the scope depends on
  // whether this script is included from an html file in ./, subscope1/, or
  // subscope2/.
  return navigator.serviceWorker.register('service_worker.js', {
    scope: './'
  }).then(swRegistrationReady).then(() => {
    return 'ok - service worker registered';
  }).catch(formatError);
}

function unregisterServiceWorker() {
  return navigator.serviceWorker.getRegistration()
    .then(function(swRegistration) {
      return swRegistration.unregister();
    }).then(function(result) {
      return 'service worker unregistration status: ' + result;
    })
  .catch(formatError);
}

function replaceServiceWorker() {
  return navigator.serviceWorker.register(
    'service_worker_with_skipWaiting_claim.js', {
    scope: './'
  }).then(swRegistrationReady).then(() => {
    return 'ok - service worker replaced';
  }).catch(formatError);
}

function removeManifest() {
  var element = document.querySelector('link[rel="manifest"]');
  if (element)
    element.parentNode.removeChild(element);
  return 'manifest removed';
}

function swapManifestNoSenderId() {
  var element = document.querySelector('link[rel="manifest"]');
  if (element) {
    element.href = 'manifest_no_sender_id.json';
    return 'sender id removed from manifest';
  } else {
    return 'unable to find manifest element';
  }
}

// This is the old style of push subscriptions which we are phasing away
// from, where the subscription used a sender ID instead of public key.
function documentSubscribePushWithoutKey() {
  return navigator.serviceWorker.ready.then(function(swRegistration) {
    return swRegistration.pushManager.subscribe({userVisibleOnly: true});
  }).then(function(subscription) {
    return subscription.endpoint;
  }).catch(formatError);
}

function documentSubscribePushWithEmptyOptions() {
  return navigator.serviceWorker.ready.then(function(swRegistration) {
    return swRegistration.pushManager.subscribe();
  }).then(function(subscription) {
    return subscription.endpoint;
  }).catch(formatError);
}

function documentSubscribePush() {
  return navigator.serviceWorker.ready.then(function(swRegistration) {
    return swRegistration.pushManager.subscribe({
          userVisibleOnly: true,
          applicationServerKey: kApplicationServerKey.buffer
        });
  }).then(function(subscription) {
    return subscription.endpoint;
  }).catch(formatError);
}

function documentSubscribePushWithNumericKey() {
  return navigator.serviceWorker.ready.then(function(swRegistration) {
    return swRegistration.pushManager.subscribe({
          userVisibleOnly: true,
          applicationServerKey: new TextEncoder().encode('1234567890')
        });
  }).then(function(subscription) {
    return subscription.endpoint;
  }).catch(formatError);
}

function documentSubscribePushWithBase64URLEncodedString() {
  return navigator.serviceWorker.ready.then(function(swRegistration) {
    return swRegistration.pushManager.subscribe({
          userVisibleOnly: true,
          applicationServerKey: kBase64URLEncodedKey
        });
  }).then(function(subscription) {
    return subscription.endpoint;
  }).catch(formatError);
}

function documentSubscribePushGetExpirationTime() {
  return navigator.serviceWorker.ready.then(function(swRegistration) {
    return swRegistration.pushManager.subscribe({
          userVisibleOnly: true,
          applicationServerKey: kApplicationServerKey.buffer
        });
  }).then(function(subscription) {
    return String(subscription.expirationTime);
  }).catch(formatError);
}

function workerSubscribePush() {
  // Send the message to the worker for it to subscribe
  return new Promise(resolve => {
    navigator.serviceWorker.addEventListener('message', resolve, false);
    navigator.serviceWorker.controller.postMessage(
      {command: 'workerSubscribe'});
  }).then((event) => JSON.parse(event.data).data);
}

function workerSubscribePushNoKey() {
  // The worker will try to subscribe without providing a key. This should
  // succeed if the worker was previously subscribed with a numeric key
  // and fail otherwise.
  return new Promise(resolve => {
    navigator.serviceWorker.addEventListener('message', resolve, false);
    navigator.serviceWorker.controller.postMessage(
      {command: 'workerSubscribeNoKey'});
  }).then((event) => JSON.parse(event.data).data);
}

function workerSubscribePushWithNumericKey(numericKey = '1234567890') {
  // Send the message to the worker for it to subscribe with the given numeric key
  return new Promise(resolve => {
    navigator.serviceWorker.addEventListener('message', resolve, false);
    navigator.serviceWorker.controller.postMessage(
      {command: 'workerSubscribeWithNumericKey', key: numericKey});
  }).then((event) => JSON.parse(event.data).data);
}

function workerSubscribePushWithBase64URLEncodedString() {
  // Send the message to the worker for it to subscribe with the given Base64URLEncoded key
  return new Promise(resolve => {
    navigator.serviceWorker.addEventListener('message', resolve, false);
    navigator.serviceWorker.controller.postMessage(
        {command: 'workerSubscribePushWithBase64URLEncodedString',
          key: kBase64URLEncodedKey});
  }).then((event) => JSON.parse(event.data).data);
}

function GetP256dh() {
  return navigator.serviceWorker.ready.then(function(swRegistration) {
    return swRegistration.pushManager.getSubscription();
  }).then(function(subscription) {
    return btoa(String.fromCharCode.apply(null,
        new Uint8Array(subscription.getKey('p256dh'))));
  }).catch(formatError);
}

function GetSubscriptionExpirationTime() {
  return navigator.serviceWorker.ready.then(function(swRegistration) {
    return swRegistration.pushManager.getSubscription();
  }).then(function(subscription) {
    return String(subscription.expirationTime);
  }).catch(formatError);
}

function pushManagerPermissionState() {
  return navigator.serviceWorker.ready.then(function(swRegistration) {
    return swRegistration.pushManager.permissionState({userVisibleOnly: true});
  }).then(function(permission) {
    return 'permission status - ' + permission;
  }).catch(formatError);
}

function notificationPermissionState() {
  return 'permission status - ' + Notification.permission;
}

function notificationPermissionAPIState() {
  return navigator.permissions.query({name: 'notifications'}).then(
      permission_status => {
    return 'permission status - ' + permission_status.state;
  }).catch(formatError);
}

function isControlled() {
  if (navigator.serviceWorker.controller) {
    return 'true - is controlled';
  } else {
    return 'false - is not controlled';
  }
}

async function unsubscribePush() {
  const swRegistration = await navigator.serviceWorker.ready;
  if (!swRegistration) {
    return 'unsubscribe result: false';
  }
  const pushSubscription = await swRegistration.pushManager.getSubscription();
  if (!pushSubscription) {
    return 'unsubscribe result: false';
  }
  try {
    const result = await pushSubscription.unsubscribe();
    return 'unsubscribe result: ' + result;
  } catch(error) {
    return 'unsubscribe error: ' + error.message;
  }
}

function storePushSubscription() {
  return navigator.serviceWorker.ready.then(swRegistration => {
    return swRegistration.pushManager.getSubscription();
  }).then(pushSubscription => {
    window.storedPushSubscription = pushSubscription;
    return 'ok - stored';
  })
  .catch(formatError);
}

function unsubscribeStoredPushSubscription() {
  return window.storedPushSubscription.unsubscribe().then(function(result) {
    return 'unsubscribe result: ' + result;
  }, function(error) {
    return 'unsubscribe error: ' + error.message;
  });
}

function hasSubscription() {
  return navigator.serviceWorker.ready.then(function(swRegistration) {
    return swRegistration.pushManager.getSubscription();
  }).then(function(subscription) {
    return subscription ? 'true - subscribed'
                        : 'false - not subscribed';
  }).catch(formatError);
}

navigator.serviceWorker.addEventListener('message', function(event) {
  var message = JSON.parse(event.data);
  if (message.type === 'push') {
    resultQueue.push(message.data);
  } else if (message.type === 'pushsubscriptionchange') {
    resultQueue.push(message.data.oldEndpoint);
    resultQueue.push(message.data.newEndpoint);
  }
}, false);
