// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function checkNotifications() {
    navigator.permissions.query({name: 'notifications'})
        .then(function(permission) {
          if (permission.state === 'prompt') {
            chrome.test.succeed();
          } else {
            chrome.test.fail();
          }
        })
  },
  function requestNotifications() {
    Notification.requestPermission().then(function(permission) {
      if (permission === 'granted') {
        chrome.test.succeed();
      } else {
        chrome.test.fail();
      }
    });
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
  },
  function geolocation_getCurrentPosition() {
    navigator.geolocation.getCurrentPosition(
        chrome.test.succeed, chrome.test.fail);
  },
  function geolocation_watchPosition() {
    navigator.geolocation.watchPosition(chrome.test.succeed, chrome.test.fail);
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
  function checkMicrophone() {
    navigator.permissions.query({name: 'microphone'})
        .then(function(permission) {
          if (permission.state === 'prompt') {
            chrome.test.succeed();
          } else {
            chrome.test.fail();
          }
        })
  },
  function requestCameraAndMicrophone() {
    if (navigator.mediaDevices) {
      navigator.mediaDevices.getUserMedia({audio: true, video: true})
          .then(function(stream) {
            chrome.test.succeed();
          })
          .catch(function(err) {
            chrome.test.fail();
          });

    } else {
      chrome.test.fail();
    }
  },
]);
