// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // Notifications permissions is denied.
  function checkNotifications() {
    navigator.permissions.query({name: 'notifications'})
        .then(function(permission) {
          if (permission.state === 'denied') {
            chrome.test.succeed();
          } else {
            chrome.test.fail();
          }
        })
  },
  // Notifications permission request is not allowed.
  function requestNotifications() {
    Notification.requestPermission().then(function(permission) {
      if (permission === 'denied') {
        chrome.test.succeed();
      } else {
        chrome.test.fail();
      }
    });
  },
  // Geolocation permissions is denied.
  function checkGeolocation() {
    navigator.permissions.query({name: 'geolocation'})
        .then(function(permission) {
          if (permission.state === 'denied') {
            chrome.test.succeed();
          } else {
            chrome.test.fail();
          }
        })
  },
  // Geolocation permission request is not allowed.
  function geolocation_getCurrentPosition() {
    navigator.geolocation.getCurrentPosition(
        chrome.test.fail, chrome.test.succeed);
  },
  function geolocation_watchPosition() {
    navigator.geolocation.watchPosition(chrome.test.fail, chrome.test.succeed);
  },
  function checkCamera() {
    navigator.permissions.query({name: 'camera'}).then(function(permission) {
      if (permission.state === 'denied') {
        chrome.test.succeed();
      } else {
        chrome.test.fail();
      }
    })
  },
  function requestCamera() {
    if (navigator.mediaDevices) {
      chrome.test.fail();
    } else {
      chrome.test.succeed();
    }
  },
  function checkMicrophone() {
    navigator.permissions.query({name: 'microphone'})
        .then(function(permission) {
          if (permission.state === 'denied') {
            chrome.test.succeed();
          } else {
            chrome.test.fail();
          }
        })
  }
]);
