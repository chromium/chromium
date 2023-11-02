// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A guest that requests permissions.
// Notifies the embedder about the result of the request (success/fail)
// via post message. Note that the embedder has to initiate a postMessage
// first so that guest has a reference to the embedder's window.

// The window reference of the embedder to send post message reply.
var embedderWindowChannel = null;

var g_testName = 'uninitialized';

var notifyEmbedder = function(status) {
  var responseArray = [g_testName, status];
  postToEmbedder(responseArray);
};


var postToEmbedder = function(msg_array) {
  embedderWindowChannel.postMessage(JSON.stringify(msg_array), '*');
};

var startGeolocationTest = function() {
  navigator.geolocation.getCurrentPosition(
      _ => {notifyEmbedder('access-granted')},
      _ => {notifyEmbedder('access-denied')});
};

var startCameraTest = function() {
  var constraints = {video: true};
  navigator.mediaDevices.getUserMedia(constraints)
      .then(function(stream) {
        notifyEmbedder('access-granted');
      })
      .catch(function(err) {
        notifyEmbedder('access-denied');
      });
};

var startMicrophoneTest = function() {
  var constraints = {audio: true};
  navigator.mediaDevices.getUserMedia(constraints)
      .then(function(stream) {
        notifyEmbedder('access-granted');
      })
      .catch(function(err) {
        notifyEmbedder('access-denied');
      });
};

var startMediaTest = function() {
  var constraints = {audio: true, video: true};
  navigator.mediaDevices.getUserMedia(constraints)
      .then(function(stream) {
        notifyEmbedder('access-granted');
      })
      .catch(function(err) {
        notifyEmbedder('access-denied');
      });
};


var onPostMessageReceived = function(e) {
  window.console.log('guest.onPostMessageReceived');
  var data = JSON.parse(e.data);
  if (data[0] == 'check-permissions') {
    g_testName = data[1];
    embedderWindowChannel = e.source;
    // Start the test once we have |embedderWindowChannel|.
    if (g_testName === 'testGeolocation') {
      startGeolocationTest();
    } else if (g_testName === 'testCamera') {
      startCameraTest();
    } else if (g_testName === 'testMicrophone') {
      startMicrophoneTest();
    } else if (g_testName === 'testMedia') {
      startMediaTest();
    } else {
      notifyEmbedder('fail');
    }
  }
};
window.addEventListener('message', onPostMessageReceived, false);
