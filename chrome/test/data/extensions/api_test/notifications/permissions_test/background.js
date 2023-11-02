// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Returns a promise that resolves once a message has been received on the given
// |messagePort|. The event handler will then remove itself.
function onNextMessage(messagePort) {
  return new Promise(resolve => {
    function listener(event) {
      resolve(event.data);
      messagePort.removeEventListener('message', listener);
    }

    messagePort.addEventListener('message', listener);
  });
}

// Registers a Service Worker and waits until it's been activated. It then
// establishes a message port so that it can be conveniently communicated with.
async function getActivatedInstrumentationServiceWorker() {
  const messageChannel = new MessageChannel();
  const registration =
      await navigator.serviceWorker.register('service_worker.js');

  // Wait for the |registration|'s installing worker to be activated.
  await new Promise(resolve => {
    const worker = registration.installing;

    registration.installing.addEventListener('statechange', () => {
      if (worker.state == 'activated')
        resolve();
    });
  });

  // Establish a message channel with the |registration|. Resolves when the
  // Service Worker has acknowledged being ready for instrumentation.
  await new Promise(resolve => {
    onNextMessage(messageChannel.port1).then(message => {
      chrome.test.assertEq('ready', message);
      resolve();
    });

    registration.active.postMessage(
        messageChannel.port2, [ messageChannel.port2 ]);

    messageChannel.port1.start();
  });

  return [ registration, messageChannel.port1 ];
}

chrome.test.getConfig(async config => {
  const expected = config.customArg;
  const [ registration, messagePort ] =
      await getActivatedInstrumentationServiceWorker();

  chrome.test.runTests([
    // Verifies that the `Notification.permission` property has the expected
    // value when called from the document context.
    function notificationPermissionDocument() {
      chrome.test.assertEq(expected, Notification.permission);
      chrome.test.succeed();
    },

    // Verifies that the `Notification.permission` property has the expected
    // value when called from the Service Worker context.
    function notificationPermissionServiceWorker() {
      onNextMessage(messagePort).then(permission => {
        chrome.test.assertEq(expected, permission);
        chrome.test.succeed();
      });

      messagePort.postMessage('notification-permission');
    },

    // Verifies that the `PushManager.permissionState()` method resolves with
    // the expected value when called from the document context.
    async function pushManagerPermissionStateDocument() {
      const permission = await registration.pushManager.permissionState({
        userVisibleOnly: true,
      });

      chrome.test.assertEq(expected, permission);
      chrome.test.succeed();
    },

    // Verifies that the `PushManager.permissionState()` method resolves with
    // the expected value when called from the Service Worker context.
    function pushManagerPermissionStateServiceWorker() {
      onNextMessage(messagePort).then(permission => {
        chrome.test.assertEq(expected, permission);
        chrome.test.succeed();
      });

      messagePort.postMessage('pushManager-permissionState');
    },

    // Verifies that the `Permissions.query()` method resolves with the expected
    // value when called from a document context.
    async function permissionsQueryDocument() {
      const permissionState = await navigator.permissions.query({
        name: 'notifications',
      });

      chrome.test.assertEq(expected, permissionState.state);
      chrome.test.succeed();
    },

    // Verifies that the `Permissions.query()` method resolves with the expected
    // value when called from a Service Worker context.
    function permissionsQueryServiceWorker() {
      onNextMessage(messagePort).then(permission => {
        chrome.test.assertEq(expected, permission);
        chrome.test.succeed();
      });

      messagePort.postMessage('permissions-query');
    },

    // Verifies that creating a non-persistent notification either succeeds or
    // fails depending on the |expected| permission status.
    function nonPersistentNotification() {
      const notification = new Notification('title', {});
      notification.addEventListener('show', () => {
        chrome.test.assertEq(expected, 'granted');
        chrome.test.succeed();
      });

      notification.addEventListener('error', () => {
        chrome.test.assertEq(expected, 'denied');
        chrome.test.succeed();
      });
    },

    // Verifies that creating a persistent notification either succeeds or fails
    // depending on the |expected| permission status.
    async function persistentNotification() {
      try {
        await registration.showNotification('title', {});
        chrome.test.assertEq(expected, 'granted');
        chrome.test.succeed();
      } catch (e) {
        chrome.test.assertEq(expected, 'denied');
        chrome.test.succeed();
      }
    }
  ]);
});
