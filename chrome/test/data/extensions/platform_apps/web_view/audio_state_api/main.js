// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var audioUrl;

function audioTests() {
  var webview = document.querySelector('webview');
  webview.getAudioState(function(audible) {
    chrome.test.assertFalse(audible);

    webview.isAudioMuted(function(audible) {
      chrome.test.assertFalse(audible);
      chrome.test.log('audio not muted by default');

      var audioChangedFalse = function(arg) {
        chrome.test.assertFalse(arg.audible);
        chrome.test.log('audiostatechanged event with audible = '
            + arg.audible + ' received');
        webview.removeEventListener('audiostatechanged', audioChangedFalse);
        webview.getAudioState(function(audible) {
          chrome.test.assertFalse(audible);
          webview.setAudioMuted(true);
          webview.isAudioMuted(function(isMuted) {
            chrome.test.assertTrue(isMuted);
            chrome.test.succeed();
          });
        });
      }

      var audioChangedTrue = function(arg) {
        chrome.test.assertTrue(arg.audible);
        chrome.test.log('audiostatechanged event with audible = '
            + arg.audible + ' received');
        webview.removeEventListener('audiostatechanged', audioChangedTrue);
        webview.addEventListener('audiostatechanged', audioChangedFalse);
        webview.getAudioState(function(audible) {
          chrome.test.assertTrue(audible);
        });
      }

      webview.addEventListener('audiostatechanged', audioChangedTrue);
      var audioCode =
          "var audio = new Audio(\"" + audioUrl + "\"); audio.play();";
      chrome.test.log('start playing audio');
      webview.executeScript({code: audioCode});
    });
  });
}

function startTest() {
  chrome.test.log('webview initializing');
  var webview = document.querySelector('webview');
  var onLoadStop = function(e) {
    webview.removeEventListener('loadstop', onLoadStop);
    audioTests();
  };

  webview.addEventListener('loadstop', onLoadStop);
  webview.src = 'data:text/html,<body>Guest</body>';
};

chrome.test.getConfig(function(config) {
  audioUrl = 'http://localhost:' + config.testServer.port +
      '/extensions/platform_apps/web_view/simple/ping.mp3';
  startTest();
});