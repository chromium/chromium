// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.addEventListener('load', function() {
  document.body.onclick = function toggleBodyFullscreen() {
    if (document.fullscreenElement || document.webkitFullscreenElement) {
      if (document.exitFullscreen)
        document.exitFullscreen();
      else if (document.webkitExitFullscreen)
        document.webkitExitFullscreen();
      else
        chrome.test.assertTrue(!"HTML5 Fullscreen API missing");
    } else {
      if (document.body.requestFullscreen)
        document.body.requestFullscreen();
      else if (document.body.webkitRequestFullscreen)
        document.body.webkitRequestFullscreen();
      else
        chrome.test.assertTrue(!"HTML5 Fullscreen API missing");
    }
  };
});

var mediaStream = null;
var events = [];

chrome.tabCapture.onStatusChanged.addListener(function(info) {
  if (info.status == 'active') {
    events.push(info.fullscreen);
    if (events.length == 3) {
      chrome.test.assertFalse(events[0]);
      chrome.test.assertTrue(events[1]);
      chrome.test.assertFalse(events[2]);
      mediaStream.getVideoTracks()[0].stop();
      mediaStream.getAudioTracks()[0].stop();
      chrome.test.notifyPass();
    }

    if (info.fullscreen)
      chrome.test.sendMessage('entered_fullscreen');
  }
});

chrome.tabCapture.capture({audio: true, video: true}, function(stream) {
  chrome.test.assertTrue(!!stream);
  mediaStream = stream;
  chrome.test.notifyPass();
  chrome.test.sendMessage('tab_capture_started');
});
