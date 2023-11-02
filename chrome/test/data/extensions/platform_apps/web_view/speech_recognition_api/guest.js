// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) {
  window.console.log(msg);
};

var embederWindowChannel = null;

var notifyEmbedder = function(msg_array) {
  embederWindowChannel.postMessage(JSON.stringify(msg_array), '*');
};

var runSpeechRecognitionAPI = function() {
  LOG('runSpeechRecognitionAPI');
  var r = new webkitSpeechRecognition();
  var succeeded = false;
  r.onstart = function() {
    succeeded = true;
    LOG('r.onstart');
    notifyEmbedder(['recognition', 'onstart', '']);
  };
  r.onerror = function() {
    LOG('r.onerror');
    if (succeeded) {
      return;
    }
    notifyEmbedder(['recognition', 'onerror', '']);
  };
  r.onresult = function(e) {
    if (!e.results || !e.results.length) {
      notifyEmbedder(['recognition', 'unknown', '']);
      return;
    }
    var transcript = e.results[0][0].transcript;
    notifyEmbedder(['recognition', 'onresult', transcript]);
  };
  r.start();
};

var onPostMessageReceived = function(e) {
  embederWindowChannel = e.source;
  var data = JSON.parse(e.data);
  if (data[0] == 'create-channel') {
    runSpeechRecognitionAPI();
  }
};

window.addEventListener('message', onPostMessageReceived);
