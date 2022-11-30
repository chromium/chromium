// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function checkNotifications() {
    navigator.permissions.query({name: 'notifications'})
        .then(function(permission) {
          if (permission.state === 'granted') {
            chrome.test.succeed();
          } else {
            chrome.test.fail();
          }
        })
  },
  function checkGeolocation() {
    navigator.permissions.query({name: 'geolocation'})
        .then(function(permission) {
          if (permission.state === 'prompt') {
            chrome.test.succeed();
          } else {
            chrome.test.fail();
          }
        })

    // Geolocation request is not allowed from a service worker.
    if (navigator.geolocation) {
      chrome.test.fail();
    }
    else {
      chrome.test.succeed();
    }
  },
  function checkCamera() {
    navigator.permissions.query({name: 'camera'}).then(function(permission) {
      if (permission.state === 'prompt') {
        chrome.test.succeed();
      } else {
        chrome.test.fail();
      }
    })
  },
  function noMediaDevicesCheck() {
    if (navigator.mediaDevices) {
      chrome.test.fail();
    } else {
      chrome.test.succeed();
    }
  },
  function checkMicrophone() {
    navigator.permissions.query({name: 'microphone'})
        .then(function(permission) {
          if (permission.state === 'prompt') {
            chrome.test.succeed();
          } else {
            chrome.test.fail();
          }
        })
  }
]);
